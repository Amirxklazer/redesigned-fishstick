// Base-game world data. DLC .dlc files use this exact same format.
#pragma once
static const char* BASE_ZONES = R"ZONES(
ZONE amir_room|Amir's Room - Main Floor
DARK 1
SAY Amir|The power just died. All of 14 is dark... even the Cleaning bots™ stopped humming.
SAY Dalia|Amir? Stay where you are, I'm in the room with you. Grab your flashlight, and watch the battery.
SAY Gabriel|(radio) Meet me in the Main hallway. Something is walking around on the lower floors. It is not one of us.
MAP
##############
#.P..........#
#.....a...B..#
#............#
#..N.........#
#............1
##############
END
NOTE a|A note in Dalia's handwriting: "If the lights go out, count to 14 and never answer anyone who knocks twice."
NPC N|Dalia|X toggles your flashlight to save battery. Hold B to run, but running is LOUD. And A on a locker lets you hide.
LINK 1 hall 1 4

ZONE hall|Main Floor Hallway
DARK 1
SAY Gabriel|There you are. The elevators are dead, so the only way down is the stairs at the east end.
SAY Gabriel|Find the parking key and a fuse. Aya says the ground gate needs both.
MAP
########################
#..........#...........#
#..........#...B....L..#
#..........#...........#
1......................3
#..........#...........#
#.M..c.....#...........#
#..........#...........#
########################
END
NOTE c|Alicia's sticky note: "Adam C. says the bots are not broken. They are LISTENING."
NPC M|Gabriel|I'll hold the hallway. Amyas swears he saw Anes go down to the Underlobby... Anes never came back.
LINK 1 amir_room 12 5
LINK 3 underlobby 2 1

ZONE underlobby|Underlobby
DARK 1
SAY Amir|The stairs smell like ash. Something here is breathing slowly.
SAY Dalia|(radio) It hears you when you run. And it sees your light, so choose.
MAP
####################
#3.......#.........#
#........#....K....#
#.d......#.........#
#..L.....#.....L...#
#.....#.......#....#
#.....#...S...#....#
#..B..#########....#
#........F.........#
#..................#
##########5#########
END
NOTE d|Anes was here: "It only moves when I stop looking. So I never stop look" ... the rest is scratched out.
LINK 3 hall 22 4
LINK 5 parking1 2 1|key|The parking gate is locked. I need the parking key.

ZONE parking1|Parking 1 - 14 Cleaning bots™
DARK 1
SAY Amir|Parking 1. Rows and rows of Cleaning bots™... every single one is facing me.
SAY Gabriel|(radio) The exit gate is at the bottom. Amir, it needs the fuse.
MAP
######################
#5...................#
#.##.##.##.##.##.##..#
#.##.##.##.##.##.##..#
#.a..L...............#
#.##.##.##.##.##.##..#
#.##.##.##.##.##.##.S#
#B...................#
#.##.##.##.##.##.##..#
#.......7......E.....#
######################
END
NOTE a|A bot's screen glows: "RESIDENT AMIR: DO NOT LEAVE. WE KEEP YOU SAFE."
LINK 5 underlobby 10 9
LINK 7 dlc_lowest 1 1||This door leads further down, but its DLC pack is missing. Put the .dlc files in sdmc:/switch/ilot14_hollow/dlc/
)ZONES";
