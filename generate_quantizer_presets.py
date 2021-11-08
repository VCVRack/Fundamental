import json
import os

scales = """
Ionian (Major)	1-2-3-4-5-6-7
Dorian	1-2-b3-4-5-6-b7
Phrygian	1-b2-b3-4-5-b6-b7
Lydian	1-2-3-#4-5-6-7
Mixolydian	1-2-3-4-5-6-b7
Aeolian (Minor)	1-2-b3-4-5-b6-b7
Locrian	1-b2-b3-4-b5-b6-b7
Aeolian 7 (Harmonic Minor)	1-2-b3-4-5-b6-7
Locrian 6	1-b2-b3-4-b5-6-b7
Ionian #5	1-2-3-4-#5-6-7
Dorian #4	1-2-b3-#4-5-6-b7
Phrygian 3	1-b2-3-4-5-b6-b7
Lydian #2	1-#2-3-#4-5-6-7
Locrian b4 bb7	1-b2-b3-b4-b5-b6-bb7
Aeolian 6 7 (Melodic Minor)	1-2-b3-4-5-6-7
Phrygian 6	1-b2-b3-4-5-6-b7
Lydian #5	1-2-3-#4-#5-6-7
Lydian b7	1-2-3-#4-5-6-b7
Aeolian 3	1-2-3-4-5-b6-b7
Locrian 2	1-2-b3-4-b5-b6-b7
Locrian b4	1-b2-b3-b4-b5-b6-b7
Bebop Dominant	1-2-3-4-5-6-#6-7
Bebop Major	1-2-3-4-5-b6-6-7
Bebop Minor	1-2-b3-3-4-5-6-b7
Bebop Melodic Minor	1-2-b3-4-5-b6-6-7
Blues Major	1-2-b3-3-5-6
Blues Minor	1-b3-4-b5-5-b7
Blues Diminished	1-b2-b3-3-b5-5-6-b7
Blues Pentatonic	1-b3-4-5-b7
Blues Rock'n'Roll	1-2-b3-3-4-b5-5-6-b7
Byzantine	1-b2-3-4-5-b6-7
Hungarian Minor	1-2-b3-b5-5-b6-7
Hungarian Gypsy	1-2-b3-#4-5-b6-b7
Spanish Gypsy	1-b2-3-4-5-b6-b7
Major Pentatonic	1-2-3-5-6
Neutral Pentatonic	1-2-4-5-b7
Rock Pentatonic	1-b3-4-#5-b7
Scottish Pentatonic	1-2-4-5-6
Minor Pentatonic	1-b3-4-5-b7
Whole	1-2-3-#4-#5-#6
Whole-Half	1-2-b3-4-#4-#5-6-7
Half-Whole	1-b2-b3-3-b5-5-6-b7
Augmented	1-#2-3-5-#5-7
Byzantine	1-b2-3-4-5-b6-7
Chromatic	1-#1-2-#2-3-4-#4-5-#5-6-#6-7
Enigmatic (Ascending)	1-b2-3-#4-#5-#6-7
Enigmatic (Descending)	1-b2-3-4-b6-b7-7
Hungarian Major	1-b3-3-b5-5-6-b7
Hungarian Minor	1-2-b3-b5-5-b6-7
Neapolitan Major	1-b2-b3-4-5-6-7
Neapolitan Minor	1-b2-b3-4-5-b6-7
Overtone	1-2-3-#4-5-6-b7
Prometheus	1-2-3-b5-6-b7
Prometheus Neapolitan	1-b2-3-b5-6-b7
Spanish 8 Tone	1-b2-b3-3-4-b5-b6-b7
"""

note_indexes = {
	"1": 0,
	"#1": 1,
	"b2": 1,
	"2": 2,
	"#2": 3,
	"b3": 3,
	"3": 4,
	"b4": 4,
	"4": 5,
	"#4": 6,
	"b5": 6,
	"5": 7,
	"#5": 8,
	"b6": 8,
	"6": 9,
	"bb7": 9,
	"#6": 10,
	"b7": 10,
	"7": 11,
}

dir = "presets/Quantizer"
os.makedirs(dir, exist_ok=True)
count = 0

for line in scales.splitlines():
	if not line:
		continue

	data = {
		"plugin": "Fundamental",
		"model": "Quantizer",
		"version": "2.0.0",
		"params": [],
		"data": {
			"enabledNotes": [
				False,
				False,
				False,
				False,
				False,
				False,
				False,
				False,
				False,
				False,
				False,
				False,
			]
		}
	}

	name, notes = line.split("\t")
	notes = notes.split("-")

	for note in notes:
		note_index = note_indexes[note]
		data["data"]["enabledNotes"][note_index] = True

	path = os.path.join(dir, f"{count:02d}_{name}.vcvm")
	print(path)
	print(data)
	with open(path, "w", newline='\n') as f:
		json.dump(data, f, indent=2)

	count += 1
