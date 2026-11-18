// ROB-21 corpus: included once in the shared region and once in the
// fragment region; its fragment-guarded sampler declaration therefore lands
// in the fragment translation unit twice (split.shared + split.fragment).
#pragma stage fragment
uniform sampler2D dup;
