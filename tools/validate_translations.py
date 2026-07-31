#!/usr/bin/env python3

import re
from glob import glob

import polib


def extractFmtFields(text):
	"""Parse fmt-style replacement fields, mirroring fmt's own escaping rules
	("{{" and "}}" are literal braces). Returns None if a brace is unmatched
	or unescaped, which fmt::format would reject at runtime with a
	format_error (crash)."""
	fields = []
	i = 0
	n = len(text)
	while i < n:
		c = text[i]
		if c == '{':
			if i + 1 < n and text[i + 1] == '{':
				i += 2
				continue
			end = text.find('}', i)
			if end == -1:
				return None
			fields.append(text[i:end + 1])
			i = end + 1
		elif c == '}':
			if i + 1 < n and text[i + 1] == '}':
				i += 2
				continue
			return None
		else:
			i += 1
	return fields


def validateEntry(original, translation):
	if translation == '':
		return True

	# Find fmt arguments in source message
	src_arguments = extractFmtFields(original)
	if src_arguments is None:
		print(f"\033[31mMalformed format string in source message: {original}\033[0m")
		return False
	if not src_arguments:
		return True

	# Find fmt arguments in translation
	translated_arguments = extractFmtFields(translation)
	if translated_arguments is None:
		print(f"\033[36m{original}\033[0m has an unmatched/unescaped brace in \033[31m{translation}\033[0m")
		return False

	# If paramteres are untyped with order, sort so that they still appear equal if reordered
	# Note: This does no hadle cases where the translator reordered arguments where not expected
	# by the source. Or other advanced but valid usages of the fmt syntax
	isOrdered = True
	for argument in src_arguments:
		if not re.search(r"^{\d+}$", argument):
			isOrdered = False
			break

	if isOrdered:
		src_arguments.sort()
		translated_arguments.sort()

	if src_arguments == translated_arguments:
		return True

	print(f"\033[36m{original}\033[0m != \033[31m{translation}\033[0m")

	return False


status = 0

files = glob('Translations/*.po')
for path in sorted(files):
	po = polib.pofile(path)
	print(f"\033[32mValidating {po.metadata['Language']}\033[0m : {po.percent_translated()}% translated")

	for entry in po:
		if entry.fuzzy:
			continue

		if entry.msgid_plural:
			for translation in entry.msgstr_plural.values():
				if not validateEntry(entry.msgid_plural, translation):
					status = 255
			continue

		if not validateEntry(entry.msgid, entry.msgstr):
			status = 255

exit(status)
