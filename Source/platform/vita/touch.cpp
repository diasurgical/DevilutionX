#include "platform/vita/touch.h"

#include <cmath>
#include <cstdint>

#include "engine/render/renderer.h"
#include "options.h"
#include "utils/display.h"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

#define TOUCH_PORT_MAX_NUM 1
#define NO_TOUCH (-1) // finger id setting if finger is not touching the screen

int visible_width;
int visible_height;
int x_borderwidth;
int y_borderwidth;

template <typename T>
inline T clip(T v, T amin, T amax)
{
	if (v < amin)
		return amin;
	if (v > amax)
		return amax;

	return v;
}

void SetMouseMotionEvent(SDL_Event *event, int32_t x, int32_t y, int32_t xrel, int32_t yrel)
{
	event->type = SDL_MOUSEMOTION;
	event->motion.x = x;
	event->motion.y = y;
	event->motion.xrel = xrel;
	event->motion.yrel = yrel;
	event->motion.which = SDL_TOUCH_MOUSEID;
}

bool touch_initialized = false;
unsigned int simulated_click_start_time[TOUCH_PORT_MAX_NUM][2]; // initiation time of last simulated left or right click (zero if no click)
bool direct_touch = true;                                       // pointer jumps to finger
Point Mouse;                                                    // always reflects current mouse position
uint32_t IgnoreEvent = SDL_USEREVENT;                           // custom event type to signal events that should be ignored

enum {
	// clang-format off
	MaxNumFingers          =   3, // number of fingers to track per panel
	MaxTapTime             = 250, // taps longer than this will not result in mouse click events
	MaxTapMotionDistance   =  10, // max distance finger motion in Vita screen pixels to be considered a tap
	SimulatedClickDuration =  50, // time in ms how long simulated mouse clicks should be
	// clang-format on
};

struct Touch {
	int id; // -1: not touching
	uint32_t timeLastDown;
	int lastX;       // last known screen coordinates
	int lastY;       // last known screen coordinates
	float lastDownX; // SDL touch coordinates when last pressed down
	float lastDownY; // SDL touch coordinates when last pressed down
};

Touch finger[TOUCH_PORT_MAX_NUM][MaxNumFingers]; // keep track of finger status

enum DraggingType : uint8_t {
	DragNone,
	DragTwoFinger,
	DragThreeFinger,
};

DraggingType multi_finger_dragging[TOUCH_PORT_MAX_NUM]; // keep track whether we are currently drag-and-dropping

void InitTouch()
{
	for (int port = 0; port < TOUCH_PORT_MAX_NUM; port++) {
		for (int i = 0; i < MaxNumFingers; i++) {
			finger[port][i].id = NO_TOUCH;
		}
		multi_finger_dragging[port] = DragNone;
	}

	for (auto &port : simulated_click_start_time) {
		for (unsigned int &time : port) {
			time = 0;
		}
	}

	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	visible_height = current.h;
	visible_width = (current.h * devilution::gnScreenWidth) / devilution::gnScreenHeight;
	x_borderwidth = (current.w - visible_width) / 2;
	y_borderwidth = (current.h - visible_height) / 2;

	IgnoreEvent = SDL_RegisterEvents(1);
}

void SetMouseButtonEvent(SDL_Event &event, uint32_t type, uint8_t button, Point position)
{
	event.type = type;
	event.button.button = button;
	if (type == SDL_MOUSEBUTTONDOWN) {
		event.button.state = SDL_PRESSED;
	} else {
		event.button.state = SDL_RELEASED;
	}
	event.button.x = position.x;
	event.button.y = position.y;
}

void PreprocessFingerDown(SDL_Event *event)
{
	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;
	// id (for multitouch)
	SDL_FingerID id = event->tfinger.fingerId;

	int x = Mouse.x;
	int y = Mouse.y;

	if (direct_touch) {
		x = static_cast<int>(event->tfinger.x * visible_width) + x_borderwidth;
		y = static_cast<int>(event->tfinger.y * visible_height) + y_borderwidth;
		devilution::OutputToLogical(&x, &y);
	}

	// make sure each finger is not reported down multiple times
	for (int i = 0; i < MaxNumFingers; i++) {
		if (finger[port][i].id != id) {
			continue;
		}
		finger[port][i].id = NO_TOUCH;
	}

	// we need the timestamps to decide later if the user performed a short tap (click)
	// or a long tap (drag)
	// we also need the last coordinates for each finger to keep track of dragging
	for (int i = 0; i < MaxNumFingers; i++) {
		if (finger[port][i].id != NO_TOUCH) {
			continue;
		}
		finger[port][i].id = id;
		finger[port][i].timeLastDown = event->tfinger.timestamp;
		finger[port][i].lastDownX = event->tfinger.x;
		finger[port][i].lastDownY = event->tfinger.y;
		finger[port][i].lastX = x;
		finger[port][i].lastY = y;
		break;
	}
}

void PreprocessBackFingerDown(SDL_Event *event)
{
	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;

	if (port != 1)
		return;

	event->type = SDL_CONTROLLERAXISMOTION;
	event->caxis.value = 32767;
	event->caxis.which = 0;
	if (event->tfinger.x <= 0.5) {
		;
		event->caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
	} else {
		event->caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
	}
}

void PreprocessBackFingerUp(SDL_Event *event)
{
	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;

	if (port != 1)
		return;

	event->type = SDL_CONTROLLERAXISMOTION;
	event->caxis.value = 0;
	event->caxis.which = 0;
	if (event->tfinger.x <= 0.5) {
		event->caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
	} else {
		event->caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
	}
}

void PreprocessFingerUp(SDL_Event *event)
{
	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;
	// id (for multitouch)
	SDL_FingerID id = event->tfinger.fingerId;

	// find out how many fingers were down before this event
	int numFingersDown = 0;
	for (int i = 0; i < MaxNumFingers; i++) {
		if (finger[port][i].id >= 0) {
			numFingersDown++;
		}
	}

	int x = Mouse.x;
	int y = Mouse.y;

	for (int i = 0; i < MaxNumFingers; i++) {
		if (finger[port][i].id != id) {
			continue;
		}

		finger[port][i].id = NO_TOUCH;
		if (multi_finger_dragging[port] == DragNone) {
			if ((event->tfinger.timestamp - finger[port][i].timeLastDown) > MaxTapTime) {
				continue;
			}

			// short (<MAX_TAP_TIME ms) tap is interpreted as right/left mouse click depending on # fingers already down
			// but only if the finger hasn't moved since it was pressed down by more than MAX_TAP_MOTION_DISTANCE pixels
			const Size outputSize = devilution::GetRenderer().GetOutputSize();
			float xrel = ((event->tfinger.x * outputSize.width) - (finger[port][i].lastDownX * outputSize.width));
			float yrel = ((event->tfinger.y * outputSize.height) - (finger[port][i].lastDownY * outputSize.height));
			auto maxRSquared = static_cast<float>(MaxTapMotionDistance * MaxTapMotionDistance);
			if ((xrel * xrel + yrel * yrel) >= maxRSquared) {
				continue;
			}

			if (numFingersDown != 2 && numFingersDown != 1) {
				continue;
			}

			Uint8 simulatedButton = 0;
			if (numFingersDown == 2) {
				simulatedButton = SDL_BUTTON_RIGHT;
				// need to raise the button later
				simulated_click_start_time[port][1] = event->tfinger.timestamp;
			} else if (numFingersDown == 1) {
				simulatedButton = SDL_BUTTON_LEFT;
				// need to raise the button later
				simulated_click_start_time[port][0] = event->tfinger.timestamp;
				if (direct_touch) {
					x = static_cast<int>(event->tfinger.x * visible_width) + x_borderwidth;
					y = static_cast<int>(event->tfinger.y * visible_height) + y_borderwidth;
					devilution::OutputToLogical(&x, &y);
				}
			}
			SetMouseButtonEvent(*event, SDL_MOUSEBUTTONDOWN, simulatedButton, { x, y });
			event->button.which = SDL_TOUCH_MOUSEID;
		} else if (numFingersDown == 1) {
			// when dragging, and the last finger is lifted, the drag is over
			Uint8 simulatedButton = 0;
			if (multi_finger_dragging[port] == DragThreeFinger) {
				simulatedButton = SDL_BUTTON_RIGHT;
			} else {
				simulatedButton = SDL_BUTTON_LEFT;
			}
			SetMouseButtonEvent(*event, SDL_MOUSEBUTTONUP, simulatedButton, { x, y });
			event->button.which = SDL_TOUCH_MOUSEID;
			multi_finger_dragging[port] = DragNone;
		}
	}
}

void PreprocessFingerMotion(SDL_Event *event)
{
	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;
	// id (for multitouch)
	SDL_FingerID id = event->tfinger.fingerId;

	// find out how many fingers were down before this event
	int numFingersDown = 0;
	for (int i = 0; i < MaxNumFingers; i++) {
		if (finger[port][i].id >= 0) {
			numFingersDown++;
		}
	}

	if (numFingersDown == 0) {
		return;
	}

	if (numFingersDown >= 1) {
		int x = Mouse.x;
		int y = Mouse.y;

		if (direct_touch) {
			x = static_cast<int>(event->tfinger.x * visible_width) + x_borderwidth;
			y = static_cast<int>(event->tfinger.y * visible_height) + y_borderwidth;
			devilution::OutputToLogical(&x, &y);
		} else {
			// for relative mode, use the pointer speed setting
			constexpr float SpeedFactor = 1.25F;

			// convert touch events to relative mouse pointer events
			// Whenever an SDL_event involving the mouse is processed,
			const Size outputSize = devilution::GetRenderer().GetOutputSize();
			x = static_cast<int>(Mouse.x + (event->tfinger.dx * SpeedFactor * outputSize.width));
			y = static_cast<int>(Mouse.y + (event->tfinger.dy * SpeedFactor * outputSize.height));
		}

		const Size clampSize = devilution::GetRenderer().GetOutputSize();
		x = clip(x, 0, clampSize.width);
		y = clip(y, 0, clampSize.height);
		int xrel = x - Mouse.x;
		int yrel = y - Mouse.y;

		// update the current finger's coordinates so we can track it later
		for (int i = 0; i < MaxNumFingers; i++) {
			if (finger[port][i].id != id)
				continue;
			finger[port][i].lastX = x;
			finger[port][i].lastY = y;
		}

		// If we are starting a multi-finger drag, start holding down the mouse button
		if (numFingersDown >= 2 && multi_finger_dragging[port] == DragNone) {
			// only start a multi-finger drag if at least two fingers have been down long enough
			int numFingersDownlong = 0;
			for (int i = 0; i < MaxNumFingers; i++) {
				if (finger[port][i].id == NO_TOUCH) {
					continue;
				}
				if (event->tfinger.timestamp - finger[port][i].timeLastDown > MaxTapTime) {
					numFingersDownlong++;
				}
			}
			if (numFingersDownlong >= 2) {
				Point mouseDown = Mouse;
				if (direct_touch) {
					for (int i = 0; i < MaxNumFingers; i++) {
						if (finger[port][i].id == id) {
							uint32_t earliestTime = finger[port][i].timeLastDown;
							for (int j = 0; j < MaxNumFingers; j++) {
								if (finger[port][j].id >= 0 && (i != j)) {
									if (finger[port][j].timeLastDown < earliestTime) {
										mouseDown.x = finger[port][j].lastX;
										mouseDown.y = finger[port][j].lastY;
										earliestTime = finger[port][j].timeLastDown;
									}
								}
							}
							break;
						}
					}
				}

				Uint8 simulatedButton = 0;
				if (numFingersDownlong == 2) {
					simulatedButton = SDL_BUTTON_LEFT;
					multi_finger_dragging[port] = DragTwoFinger;
				} else {
					simulatedButton = SDL_BUTTON_RIGHT;
					multi_finger_dragging[port] = DragThreeFinger;
				}
				SDL_Event ev;
				SetMouseButtonEvent(ev, SDL_MOUSEBUTTONDOWN, simulatedButton, mouseDown);
				ev.button.which = SDL_TOUCH_MOUSEID;
				SDL_PushEvent(&ev);
			}
		}

		if (xrel == 0 && yrel == 0) {
			return;
		}

		// check if this is the "oldest" finger down (or the only finger down)
		// otherwise it will not affect mouse motion
		bool updatePointer = true;
		if (numFingersDown > 1) {
			for (int i = 0; i < MaxNumFingers; i++) {
				if (finger[port][i].id != id) {
					continue;
				}
				for (int j = 0; j < MaxNumFingers; j++) {
					if (finger[port][j].id == NO_TOUCH || (j == i)) {
						continue;
					}
					if (finger[port][j].timeLastDown < finger[port][i].timeLastDown) {
						updatePointer = false;
					}
				}
			}
		}
		if (!updatePointer) {
			return;
		}
		SetMouseMotionEvent(event, x, y, xrel, yrel);
	}
}

void PreprocessEvents(SDL_Event *event)
{
	// Supported touch gestures:
	// left mouse click: single finger short tap
	// right mouse click: second finger short tap while first finger is still down
	// pointer motion: single finger drag
	// left button drag and drop: dual finger drag
	// right button drag and drop: triple finger drag
	if (event->type != SDL_FINGERDOWN && event->type != SDL_FINGERUP && event->type != SDL_FINGERMOTION) {
		return;
	}

	// front (0) or back (1) panel
	SDL_TouchID port = event->tfinger.touchId;
	if (port != 0) {
		if (devilution::GetOptions().Controller.bRearTouch) {
			switch (event->type) {
			case SDL_FINGERDOWN:
				PreprocessBackFingerDown(event);
				break;
			case SDL_FINGERUP:
				PreprocessBackFingerUp(event);
				break;
			}
		}
		return;
	}

	switch (event->type) {
	case SDL_FINGERDOWN:
		PreprocessFingerDown(event);
		break;
	case SDL_FINGERUP:
		PreprocessFingerUp(event);
		break;
	case SDL_FINGERMOTION:
		PreprocessFingerMotion(event);
		break;
	}
}

} // namespace

void HandleTouchEvent(SDL_Event *event, Point mousePosition)
{
	Mouse = mousePosition;

	if (!touch_initialized) {
		InitTouch();
		touch_initialized = true;
	}
	PreprocessEvents(event);
	if (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP || event->type == SDL_FINGERMOTION) {
		event->type = IgnoreEvent;
	}
}

void FinishSimulatedMouseClicks(Point mousePosition)
{
	Mouse = mousePosition;

	for (auto &port : simulated_click_start_time) {
		for (int i = 0; i < 2; i++) {
			if (port[i] == 0) {
				continue;
			}

			Uint32 currentTime = SDL_GetTicks();
			if (currentTime - port[i] < SimulatedClickDuration) {
				continue;
			}

			int simulatedButton;
			if (i == 0) {
				simulatedButton = SDL_BUTTON_LEFT;
			} else {
				simulatedButton = SDL_BUTTON_RIGHT;
			}
			SDL_Event ev;
			SetMouseButtonEvent(ev, SDL_MOUSEBUTTONUP, simulatedButton, Mouse);
			ev.button.which = SDL_TOUCH_MOUSEID;
			SDL_PushEvent(&ev);

			port[i] = 0;
		}
	}
}

} // namespace devilution
