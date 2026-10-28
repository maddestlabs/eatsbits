#include "eatsbits/project/sequence_catalog.hpp"
#include <algorithm>

namespace eatsbits::project {

const std::vector<SequenceDefinition>& SequenceCatalog::getAllSequences() {
    static const std::vector<SequenceDefinition> s_sequences = {
        // --- 1. Classic Acid 303 Riff ---
        {
            "acid_seq_01",
            "Classic Acid 303 Riff",
            "MIDI_SEQ",
            "Acid",
            "16-Step energetic 16th-note acid bassline with authentic slides and accents",
            "acid, 303, bassline, sequence, roland",
            "-- Clip: \"Classic Acid 303 Riff\" | Events: 16\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 0.9, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 1.0, dur = 0.8, vel = 0.70 }\n"
            "{ pitch = \"C3\", step = 2.0, dur = 0.9, vel = 0.90, slide = true }\n"
            "{ pitch = \"A#2\", step = 3.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"C2\", step = 4.0, dur = 0.9, vel = 0.95, accent = true }\n"
            "{ pitch = \"D#2\", step = 5.0, dur = 0.8, vel = 0.70 }\n"
            "{ pitch = \"F2\", step = 6.0, dur = 0.9, vel = 0.85, slide = true }\n"
            "{ pitch = \"G2\", step = 7.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"C2\", step = 8.0, dur = 0.9, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 9.0, dur = 0.8, vel = 0.70 }\n"
            "{ pitch = \"D#3\", step = 10.0, dur = 0.9, vel = 0.90, slide = true }\n"
            "{ pitch = \"D3\", step = 11.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"A#2\", step = 12.0, dur = 0.9, vel = 0.90 }\n"
            "{ pitch = \"G2\", step = 13.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"F2\", step = 14.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"D#2\", step = 15.0, dur = 0.8, vel = 0.75 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 2. Techno 4-on-the-Floor Full Groove ---
        {
            "four_floor_seq",
            "Techno 4-on-the-Floor",
            "MIDI_SEQ",
            "Techno",
            "Standard 909 kick, snappy snare and open/closed hats",
            "techno, drums, 909, 4-to-floor, beat",
            "-- Clip: \"Techno 4-on-the-Floor\" | Events: 18\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "-- Kick on downbeats (1, 2, 3, 4)\n"
            "{ pitch = \"C2\", step = 0.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 4.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 8.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 12.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "-- Snare on beats 2 and 4\n"
            "{ pitch = \"D2\", step = 4.0, dur = 1.0, vel = 0.90, lane = 1, accent = true }\n"
            "{ pitch = \"D2\", step = 12.0, dur = 1.0, vel = 0.90, lane = 1, accent = true }\n"
            "-- Closed Hats on 8ths\n"
            "{ pitch = \"F#2\", step = 0.0, dur = 0.5, vel = 0.70, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 2.0, dur = 0.5, vel = 0.60, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 4.0, dur = 0.5, vel = 0.70, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 6.0, dur = 0.5, vel = 0.60, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 8.0, dur = 0.5, vel = 0.70, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 10.0, dur = 0.5, vel = 0.60, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 12.0, dur = 0.5, vel = 0.70, lane = 2 }\n"
            "{ pitch = \"F#2\", step = 14.0, dur = 0.5, vel = 0.60, lane = 2 }\n"
            "-- Open Hats on offbeats\n"
            "{ pitch = \"A#2\", step = 2.0, dur = 1.5, vel = 0.85, lane = 3, accent = true }\n"
            "{ pitch = \"A#2\", step = 6.0, dur = 1.5, vel = 0.85, lane = 3, accent = true }\n"
            "{ pitch = \"A#2\", step = 10.0, dur = 1.5, vel = 0.85, lane = 3, accent = true }\n"
            "{ pitch = \"A#2\", step = 14.0, dur = 1.5, vel = 0.85, lane = 3, accent = true }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 3. 4-to-Floor Kick ---
        {
            "seq_4_to_floor",
            "4-to-Floor Kick",
            "MIDI_SEQ",
            "Kick",
            "Classic four-on-the-floor kick drum pattern hitting on every downbeat (1, 2, 3, 4)",
            "kick, drums, 4-on-the-floor, edm, house",
            "-- Clip: \"4-to-Floor Kick\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 4.0, dur = 1.0, vel = 0.90 }\n"
            "{ pitch = \"C2\", step = 8.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 12.0, dur = 1.0, vel = 0.90 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 4. Syncopated Break Kick ---
        {
            "seq_kick_break",
            "Syncopated Break Kick",
            "MIDI_SEQ",
            "Kick",
            "Syncopated funk/breakbeat kick drum pattern on steps 1, 7, and 11",
            "kick, breakbeat, funk, hiphop, drums",
            "-- Clip: \"Syncopated Break Kick\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"C2\", step = 6.0, dur = 1.0, vel = 0.85 }\n"
            "{ pitch = \"C2\", step = 10.0, dur = 1.0, vel = 0.90, accent = true }\n"
            "{ pitch = \"C2\", step = 15.0, dur = 0.5, vel = 0.70 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 5. 8th-Note Closed Hats ---
        {
            "seq_eighth_hats",
            "8th-Note Closed Hats",
            "MIDI_SEQ",
            "Hi-Hat",
            "Driving 8th-note closed hi-hat groove with dynamic accents on the downbeats",
            "hihat, hats, drums, groove, 8th",
            "-- Clip: \"8th-Note Closed Hats\" | Events: 8\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"F#2\", step = 0.0, dur = 0.75, vel = 0.90, accent = true }\n"
            "{ pitch = \"F#2\", step = 2.0, dur = 0.75, vel = 0.65 }\n"
            "{ pitch = \"F#2\", step = 4.0, dur = 0.75, vel = 0.85, accent = true }\n"
            "{ pitch = \"F#2\", step = 6.0, dur = 0.75, vel = 0.65 }\n"
            "{ pitch = \"F#2\", step = 8.0, dur = 0.75, vel = 0.90, accent = true }\n"
            "{ pitch = \"F#2\", step = 10.0, dur = 0.75, vel = 0.65 }\n"
            "{ pitch = \"F#2\", step = 12.0, dur = 0.75, vel = 0.85, accent = true }\n"
            "{ pitch = \"F#2\", step = 14.0, dur = 0.75, vel = 0.70 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 6. Offbeat Open Hats ---
        {
            "seq_offbeat_hats",
            "Offbeat Open Hats",
            "MIDI_SEQ",
            "Hi-Hat",
            "Classic open hi-hat on the offbeats (and of each beat) for driving house & techno energy",
            "hihat, openhat, house, techno, upbeat",
            "-- Clip: \"Offbeat Open Hats\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"A#2\", step = 2.0, dur = 1.5, vel = 0.88, accent = true }\n"
            "{ pitch = \"A#2\", step = 6.0, dur = 1.5, vel = 0.85 }\n"
            "{ pitch = \"A#2\", step = 10.0, dur = 1.5, vel = 0.88, accent = true }\n"
            "{ pitch = \"A#2\", step = 14.0, dur = 1.5, vel = 0.85 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 7. Quarter-Note Hats ---
        {
            "seq_quarter_hats",
            "Quarter-Note Hats",
            "MIDI_SEQ",
            "Hi-Hat",
            "Solid quarter-note closed hi-hats on every downbeat (1, 2, 3, 4)",
            "hihat, quarter, drums, rhythm",
            "-- Clip: \"Quarter-Note Hats\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"F#2\", step = 0.0, dur = 1.0, vel = 0.85 }\n"
            "{ pitch = \"F#2\", step = 4.0, dur = 1.0, vel = 0.80 }\n"
            "{ pitch = \"F#2\", step = 8.0, dur = 1.0, vel = 0.85 }\n"
            "{ pitch = \"F#2\", step = 12.0, dur = 1.0, vel = 0.80 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 8. 16th Trap Hat Roll ---
        {
            "seq_trap_hats",
            "16th Trap Hat Roll",
            "MIDI_SEQ",
            "Hi-Hat",
            "Trap-style 16th-note closed hi-hat sequence with velocity rolling and a 32nd-note burst",
            "trap, hihat, roll, hiphop, 16th",
            "-- Clip: \"16th Trap Hat Roll\" | Events: 18\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"F#2\", step = 0.0, dur = 0.5, vel = 0.85, accent = true }\n"
            "{ pitch = \"F#2\", step = 1.0, dur = 0.5, vel = 0.60 }\n"
            "{ pitch = \"F#2\", step = 2.0, dur = 0.5, vel = 0.75 }\n"
            "{ pitch = \"F#2\", step = 3.0, dur = 0.5, vel = 0.60 }\n"
            "{ pitch = \"F#2\", step = 4.0, dur = 0.5, vel = 0.80, accent = true }\n"
            "{ pitch = \"F#2\", step = 5.0, dur = 0.5, vel = 0.65 }\n"
            "{ pitch = \"F#2\", step = 6.0, dur = 0.5, vel = 0.70 }\n"
            "{ pitch = \"F#2\", step = 7.0, dur = 0.5, vel = 0.60 }\n"
            "{ pitch = \"F#2\", step = 8.0, dur = 0.5, vel = 0.85, accent = true }\n"
            "{ pitch = \"F#2\", step = 9.0, dur = 0.5, vel = 0.65 }\n"
            "{ pitch = \"F#2\", step = 10.0, dur = 0.5, vel = 0.75 }\n"
            "{ pitch = \"F#2\", step = 11.0, dur = 0.5, vel = 0.60 }\n"
            "{ pitch = \"F#2\", step = 12.0, dur = 0.5, vel = 0.80, accent = true }\n"
            "{ pitch = \"F#2\", step = 13.0, dur = 0.5, vel = 0.70 }\n"
            "{ pitch = \"F#2\", step = 14.0, dur = 0.25, vel = 0.90 }\n"
            "{ pitch = \"F#2\", step = 14.5, dur = 0.25, vel = 0.80 }\n"
            "{ pitch = \"F#2\", step = 15.0, dur = 0.25, vel = 0.85 }\n"
            "{ pitch = \"F#2\", step = 15.5, dur = 0.25, vel = 0.75 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 9. Backbeat Snare ---
        {
            "seq_snares",
            "Backbeat Snare",
            "MIDI_SEQ",
            "Snare",
            "Standard backbeat acoustic/electronic snare hitting on beats 2 and 4",
            "snare, backbeat, drums, rhythm, 2-and-4",
            "-- Clip: \"Backbeat Snare\" | Events: 2\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"D2\", step = 4.0, dur = 1.0, vel = 0.92, accent = true }\n"
            "{ pitch = \"D2\", step = 12.0, dur = 1.0, vel = 0.95, accent = true }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 10. Trap Snare & Ghost ---
        {
            "seq_trap_snare",
            "Trap Snare & Ghost",
            "MIDI_SEQ",
            "Snare",
            "Crisp trap snare on beats 2 and 4 with syncopated ghost notes and pickup roll",
            "snare, trap, ghost, roll, hiphop",
            "-- Clip: \"Trap Snare & Ghost\" | Events: 5\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"D2\", step = 4.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"D2\", step = 9.0, dur = 0.5, vel = 0.45 }\n"
            "{ pitch = \"D2\", step = 11.0, dur = 0.5, vel = 0.55 }\n"
            "{ pitch = \"D2\", step = 12.0, dur = 1.0, vel = 0.95, accent = true }\n"
            "{ pitch = \"D2\", step = 15.0, dur = 0.5, vel = 0.50 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 11. Dance Clap ---
        {
            "seq_disco_clap",
            "Dance Clap",
            "MIDI_SEQ",
            "Snare",
            "House and disco clap on beats 2 and 4 with layered flam",
            "clap, disco, house, dance, percussion",
            "-- Clip: \"Dance Clap\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"D#2\", step = 3.9, dur = 0.8, vel = 0.60 }\n"
            "{ pitch = \"D#2\", step = 4.0, dur = 1.0, vel = 0.95, lane = 1, accent = true }\n"
            "{ pitch = \"D#2\", step = 11.9, dur = 0.8, vel = 0.60 }\n"
            "{ pitch = \"D#2\", step = 12.0, dur = 1.0, vel = 0.95, lane = 1, accent = true }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 12. Sustained Root Bass ---
        {
            "seq_bass_full",
            "Sustained Root Bass",
            "MIDI_SEQ",
            "Bassline",
            "Single sustained full 1-bar root bass note (16 steps) for deep sub and synth bass",
            "bass, sub, sustained, root, 1-bar",
            "-- Clip: \"Sustained Root Bass\" | Events: 1\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 16.0, vel = 0.85 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 13. Half-Note Pumping Bass ---
        {
            "seq_bass_halves",
            "Half-Note Pumping Bass",
            "MIDI_SEQ",
            "Bassline",
            "Two half-bar sustained bass notes (8 steps each) alternating between root and fifth",
            "bass, halfnote, root, fifth, pumping",
            "-- Clip: \"Half-Note Pumping Bass\" | Events: 2\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 8.0, vel = 0.88, accent = true }\n"
            "{ pitch = \"G1\", step = 8.0, dur = 8.0, vel = 0.82 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 14. Offbeat Rolling Bass ---
        {
            "seq_offbeat_bass",
            "Offbeat Rolling Bass",
            "MIDI_SEQ",
            "Bassline",
            "Classic psytrance and eurodance offbeat bassline driving on every upbeat (2, 6, 10, 14)",
            "bass, offbeat, trance, edm, rolling",
            "-- Clip: \"Offbeat Rolling Bass\" | Events: 4\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 2.0, dur = 1.5, vel = 0.90, accent = true }\n"
            "{ pitch = \"C2\", step = 6.0, dur = 1.5, vel = 0.85 }\n"
            "{ pitch = \"C2\", step = 10.0, dur = 1.5, vel = 0.90, accent = true }\n"
            "{ pitch = \"C2\", step = 14.0, dur = 1.5, vel = 0.85 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 15. Dynamic 16th Bassline ---
        {
            "seq_dynamic_bassline",
            "Dynamic 16th Bassline",
            "MIDI_SEQ",
            "Bassline",
            "Funky, syncopated 16th-note bassline pattern moving across the minor pentatonic scale",
            "bass, funk, syncopated, dynamic, groove",
            "-- Clip: \"Dynamic 16th Bassline\" | Events: 10\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C2\", step = 0.0, dur = 1.5, vel = 0.92, accent = true }\n"
            "{ pitch = \"C2\", step = 2.0, dur = 0.8, vel = 0.70 }\n"
            "{ pitch = \"D#2\", step = 3.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"F2\", step = 5.0, dur = 1.0, vel = 0.85, accent = true }\n"
            "{ pitch = \"G2\", step = 7.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"C2\", step = 8.0, dur = 1.5, vel = 0.92, accent = true }\n"
            "{ pitch = \"A#1\", step = 10.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"C2\", step = 11.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"G2\", step = 13.0, dur = 1.0, vel = 0.85 }\n"
            "{ pitch = \"A#2\", step = 15.0, dur = 0.8, vel = 0.75, slide = true }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 16. Chord Stabs ---
        {
            "seq_chord_stabs",
            "Chord Stabs",
            "MIDI_SEQ",
            "Chords",
            "Dynamic polyphonic C minor chord stabs hitting on offbeats for classic house & dub vibes",
            "chords, stabs, polyphonic, house, dub",
            "-- Clip: \"Chord Stabs\" | Events: 12\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C4\", step = 2.0, dur = 1.0, vel = 0.85, accent = true }\n"
            "{ pitch = \"D#4\", step = 2.0, dur = 1.0, vel = 0.80, lane = 1 }\n"
            "{ pitch = \"G4\", step = 2.0, dur = 1.0, vel = 0.82, lane = 2 }\n"
            "{ pitch = \"C4\", step = 6.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"D#4\", step = 6.0, dur = 0.8, vel = 0.70, lane = 1 }\n"
            "{ pitch = \"G4\", step = 6.0, dur = 0.8, vel = 0.72, lane = 2 }\n"
            "{ pitch = \"C4\", step = 10.0, dur = 1.0, vel = 0.85, accent = true }\n"
            "{ pitch = \"D#4\", step = 10.0, dur = 1.0, vel = 0.80, lane = 1 }\n"
            "{ pitch = \"G4\", step = 10.0, dur = 1.0, vel = 0.82, lane = 2 }\n"
            "{ pitch = \"C4\", step = 14.0, dur = 0.8, vel = 0.78 }\n"
            "{ pitch = \"D#4\", step = 14.0, dur = 0.8, vel = 0.72, lane = 1 }\n"
            "{ pitch = \"G4\", step = 14.0, dur = 0.8, vel = 0.75, lane = 2 }\n",
            0.0f, 0.90f, 0.46f
        },

        // --- 17. Synth Arpeggio Run ---
        {
            "seq_arpeggio_lead",
            "Synth Arpeggio Run",
            "MIDI_SEQ",
            "Arpeggio",
            "16th-note ascending and descending synthesizer arpeggio line over C minor pentatonic",
            "arp, arpeggio, lead, synth, melodic",
            "-- Clip: \"Synth Arpeggio Run\" | Events: 16\n"
            "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n"
            "{ pitch = \"C4\", step = 0.0, dur = 0.8, vel = 0.90, accent = true }\n"
            "{ pitch = \"D#4\", step = 1.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"G4\", step = 2.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"A#4\", step = 3.0, dur = 0.8, vel = 0.85 }\n"
            "{ pitch = \"C5\", step = 4.0, dur = 0.8, vel = 0.90, accent = true }\n"
            "{ pitch = \"A#4\", step = 5.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"G4\", step = 6.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"D#4\", step = 7.0, dur = 0.8, vel = 0.70 }\n"
            "{ pitch = \"C4\", step = 8.0, dur = 0.8, vel = 0.90, accent = true }\n"
            "{ pitch = \"D#4\", step = 9.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"F4\", step = 10.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"G4\", step = 11.0, dur = 0.8, vel = 0.85 }\n"
            "{ pitch = \"A#4\", step = 12.0, dur = 0.8, vel = 0.90, accent = true }\n"
            "{ pitch = \"G4\", step = 13.0, dur = 0.8, vel = 0.80 }\n"
            "{ pitch = \"F4\", step = 14.0, dur = 0.8, vel = 0.75 }\n"
            "{ pitch = \"D#4\", step = 15.0, dur = 0.8, vel = 0.70 }\n",
            0.0f, 0.90f, 0.46f
        }
    };
    return s_sequences;
}

const SequenceDefinition* SequenceCatalog::findSequence(const std::string& id) {
    const auto& list = getAllSequences();
    auto it = std::find_if(list.begin(), list.end(), [&](const SequenceDefinition& s) {
        return s.id == id;
    });
    if (it != list.end()) return &(*it);
    return nullptr;
}

std::vector<eatscript::NoteScriptEngine::ClipNoteEvent> SequenceCatalog::getSequenceEvents(const std::string& id) {
    const auto* seq = findSequence(id);
    if (!seq) return {};
    return eatscript::NoteScriptEngine::parseClipEvents(seq->eatscriptCode);
}

} // namespace eatsbits::project
