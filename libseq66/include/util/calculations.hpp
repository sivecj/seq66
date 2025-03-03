#if ! defined SEQ66_CALCULATIONS_HPP
#define SEQ66_CALCULATIONS_HPP

/*
 *  This file is part of seq66.
 *
 *  seq66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  seq66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with seq66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          calculations.hpp
 *
 *  This module declares/defines some common calculations needed by the
 *  application.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2015-11-07
 * \updates       2021-08-10
 * \license       GNU GPLv2 or above
 *
 *  These items were moved from the globals.h module so that only the modules
 *  that need them need to include them.  Also included are some minor
 *  "utility" functions dealing with MIDI and port-related strings.  Many of
 *  the functions are defined in this header file, as inline code.
 */

#include <string>
#include <vector>

#include "midi/midibytes.hpp"           /* midipulse alias and much more    */

/*
 * Global functions in the seq66 namespace for MIDI timing calculations.
 */

namespace seq66
{

/**
 *  Provides a clear enumation of wave types supported by the wave function.
 *  We still have to clarify these type values, though.
 */

enum class wave
{
    none               = 0,    /**< No waveform, never used.           */
    sine               = 1,    /**< Sine wave modulation.              */
    sawtooth           = 2,    /**< Saw-tooth (ramp) modulation.       */
    reverse_sawtooth   = 3,    /**< Reverse saw-tooth (decay).         */
    triangle           = 4     /**< No waveform, never used.           */
};

/*
 * Free functions in the seq66 namespace.
 */

extern std::string wave_type_name (wave wv);
extern int extract_timing_numbers
(
    const std::string & s,
    std::string & part_1,
    std::string & part_2,
    std::string & part_3,
    std::string & fraction
);
extern int tokenize_string
(
    const std::string & source,
    std::vector<std::string> & tokens
);
extern std::string pulses_to_string (midipulse p);
extern std::string pulses_to_measurestring
(
    midipulse p,
    const midi_timing & seqparms
);
extern bool pulses_to_midi_measures
(
    midipulse p,
    const midi_timing & seqparms,
    midi_measures & measures
);
extern std::string pulses_to_timestring
(
    midipulse p,
    const midi_timing & timinginfo
);
extern std::string pulses_to_timestring
(
    midipulse pulses, midibpm bpm, int ppqn,
    bool showus = true, bool showhrs = true
);
extern midipulse measurestring_to_pulses
(
    const std::string & measures,
    const midi_timing & seqparms
);
extern midipulse midi_measures_to_pulses
(
    const midi_measures & measures,
    const midi_timing & seqparms
);
extern midipulse timestring_to_pulses
(
    const std::string & timestring,
    int bpm, int ppqn
);
extern midipulse string_to_pulses
(
    const std::string & s,
    const midi_timing & mt
);
extern int randomize (int range);
extern int log2_time_sig_value (int tsd);
extern int beat_power_of_2 (int logbase2);
extern int power (int base, int exponent);
extern midibyte beat_log2 (int value);
extern midibpm tempo_us_from_bytes (const midibyte tt[3]);
extern void tempo_us_to_bytes (midibyte t[3], int tempo_us);
extern midibyte tempo_to_note_value (midibpm tempo);
extern midibpm fix_tempo (midibpm bpm);
extern unsigned short combine_bytes (midibyte b0, midibyte b1);
extern midibpm note_value_to_tempo (midibyte note);

/**
 *  Formalizes the rescaling of ticks base on changing the PPQN.  For speed
 *  the parameters are all assumed to be valid.  The PPQN values supported
 *  explicity range from 32 to 19200.  The maximum tick value for 32-bit code
 *  is 2147483647.  At the highest PPQN that's almost 28000 measures.  64-bit
 *  code maxes at over 9E18.
 *
 *  This function is similar to pulses_scaled(), but that function always
 *  scales against SEQ66_DEFAULT_PPQN and allows for a zoom factor.
 *
 * \param tick
 *      The tick value to be rescaled.
 *
 * \param newppqn
 *      The new PPQN.
 *
 * \param oldppqn
 *      The original PPQN.  Defaults to 192.
 *
 * \return
 *      Returns the new tick value.
 */

inline midipulse
rescale_tick (midipulse tick, int newppqn, int oldppqn)
{
    return midipulse(double(tick) * newppqn / oldppqn + 0.5);
}

/**
 *  Converts tempo (e.g. 120 beats/minute) to microseconds.
 *  This function is the inverse of bpm_from_tempo_us().
 *
 * \param bpm
 *      The value of beats-per-minute.  If this value is 0, we'll get an
 *      arithmetic exception.
 *
 * \return
 *      Returns the tempo in qn/us.  If the bpm value is 0, then 0 is
 *      returned.
 */

inline double
tempo_us_from_bpm (midibpm bpm)
{
    return bpm > 0.0 ? (60000000.0 / bpm) : 0.0 ;
}

/**
 *  This function calculates the effective beats-per-minute based on the value
 *  of a Tempo meta-event.  The tempo event's numeric value is given in 3
 *  bytes, and is in units of microseconds-per-quarter-note (us/qn).
 *
 * \param tempous
 *      The value of the Tempo meta-event, in units of us/qn.  If this value
 *      is 0, we'll get an arithmetic exception.
 *
 * \return
 *      Returns the beats per minute.  If the tempo value is 0, then 0 is
 *      returned.
 */

inline midibpm
bpm_from_tempo_us (double tempous)
{
    return tempous > 0.0 ? (60000000.0 / tempous) : 0.0 ;
}

/**
 *  Provides a direct conversion from a midibyte array to the beats/minute
 *  value.
 *
 *  It might be worthwhile to provide an std::vector version at some point.
 *
 * \param t
 *      The 3 tempo midibytes that were read directly from a MIDI file.
 */

inline midibpm
bpm_from_bytes (midibyte t[3])
{
    return bpm_from_tempo_us(tempo_us_from_bytes(t));
}

/**
 *  Calculates pulse-length from the BPM (beats-per-minute) and PPQN
 *  (pulses-per-quarter-note) values.  The formula for the pulse-length in
 *  seconds is:
 *
\verbatim
                 60
        P = ------------
             BPM * PPQN
\endverbatim
 *
 * \param bpm
 *      Provides the beats-per-minute value.  No sanity check is made.  If
 *      this value is 0, we'll get an arithmetic exception.
 *
 * \param ppqn
 *      Provides the pulses-per-quarter-note value.  No sanity check is
 *      made.  If this value is 0, we'll get an arithmetic exception.
 *
 * \return
 *      Returns the pulse length in microseconds.  If either parameter is
 *      invalid, then this function will crash. :-D
 */

inline double
pulse_length_us (midibpm bpm, int ppqn)
{
    /*
     * Let's use the original notation for now.
     *
     * return 60000000.0 / double(bpm * ppqn);
     */

    return 60000000.0 / ppqn / bpm;
}

/**
 *  Converts delta time in microseconds to ticks.  This function is the
 *  inverse of ticks_to_delta_time_us().
 *
 *  Please note that terms "ticks" and "pulses" are equivalent, and refer to
 *  the "pulses" in "pulses per quarter note".
 *
\verbatim
             beats       pulses           1 minute       1 sec
    P = 120 ------ * 192 ------ * T us *  ---------  * ---------
            minute       beats            60 sec       1,000,000 us
\endverbatim
 *
 *  Note that this formula assumes that a beat is a quarter note.  If a beat
 *  is an eighth note, then the P value would be halved, because there would
 *  be only 96 pulses per beat.  We will implement an additional function to
 *  account for the beat; the current function merely blesses some
 *  calculations made in the application.
 *
 * \param us
 *      The number of microseconds in the delta time.
 *
 * \param bpm
 *      Provides the beats-per-minute value, otherwise known as the "tempo".
 *
 * \param ppqn
 *      Provides the pulses-per-quarter-note value, otherwise known as the
 *      "division".
 *
 * \return
 *      Returns the tick value.
 */

inline double
delta_time_us_to_ticks (unsigned long us, midibpm bpm, int ppqn)
{
    return double(bpm * ppqn * (us / 60000000.0f));
}

/**
 *  Converts the time in ticks ("clocks") to delta time in microseconds.
 *  The inverse of delta_time_us_to_ticks().
 *
 *  Please note that terms "ticks" and "pulses" are equivalent, and refer to
 *  the "pulses" in "pulses per quarter note".
 *
 *  Old:  60000000.0 * double(delta_ticks) / (double(bpm) * double(ppqn));
 *
 * \param delta_ticks
 *      The number of ticks or "clocks".
 *
 * \param bpm
 *      Provides the beats-per-minute value, otherwise known as the "tempo".
 *
 * \param ppqn
 *      Provides the pulses-per-quarter-note value, otherwise known as the
 *      "division".
 *
 * \return
 *      Returns the time value in microseconds.
 */

inline double
ticks_to_delta_time_us (midipulse delta_ticks, midibpm bpm, int ppqn)
{
    return double(delta_ticks) * pulse_length_us(bpm, ppqn);
}

/**
 *  The MIDI beat clock (also known as "MIDI timing clock" or "MIDI clock") is
 *  a clock signal that is broadcast via MIDI to ensure that several
 *  MIDI-enabled devices or sequencers stay in synchronization.  Do not
 *  confuse it with "MIDI timecode".
 *
 *  The standard MIDI beat clock ticks every 24 times every quarter note
 *  (crotchet).
 *
 *  Unlike MIDI timecode, the MIDI beat clock is tempo-dependent. Clock events
 *  are sent at a rate of 24 ppqn (pulses per quarter note). Those pulses are
 *  used to maintain a synchronized tempo for synthesizers that have
 *  BPM-dependent voices and also for arpeggiator synchronization.
 *  The following value represents the standard MIDI clock rate in
 *  beats-per-quarter-note.
 */

inline int
midi_clock_beats_per_qn ()
{
    return 24;
}

/**
 *  A simple calculation to convert PPQN to MIDI clock ticks, which are emitting
 *  24 times per quarter note.
 *
 * \param ppqn
 *      The number of pulses per quarter note.  For example, the default value
 *      for Seq24 is 192.
 *
 * \return
 *      The integer value of ppqn / 24 [MIDI clock PPQN] is returned.
 */

inline int
clock_ticks_from_ppqn (int ppqn)
{
    return ppqn / midi_clock_beats_per_qn();
}

/**
 *  A simple calculation to convert PPQN to MIDI clock ticks.  The same as
 *  clock_ticks_from_ppqn(), but returned as a double float.
 *
 * \param ppqn
 *      The number of pulses per quarter note.
 *
 * \return
 *      The double value of ppqn / 24 [midi_clock_beats_per_qn] is returned.
 */

inline double
double_ticks_from_ppqn (int ppqn)
{
    return ppqn / double(midi_clock_beats_per_qn());
}

/**
 *  Calculates the pulses per measure.  This calculation is extremely simple,
 *  and it provides an important constraint to pulse (ticks) calculations:
 *  the number of pulses in a measure is always 4 times the PPQN value,
 *  regardless of the time signature.  The number pulses in a 7/8 measure is
 *  the the same as in a 4/4 measure.
 */

inline midipulse
pulses_per_measure (int ppqn)
{
    return 4 * ppqn;
}

/**
 *  Calculates the length of an integral number of measures, in ticks.
 *  This function is called in seqedit::apply_length(), when the user
 *  selects a sequence length in measures.  That function calculates the
 *  length in ticks.  The number of pulses is given by the number of quarter
 *  notes times the pulses per quarter note.  The number of quarter notes is
 *  given by the measures times the quarter notes per measure.  The quarter
 *  notes per measure is given by the beats per measure times 4 divided by
 *  beat_width beats.  So:
 *
\verbatim
    p = 4 * P * m * B / W
        p == pulse count (ticks or pulses)
        m == number of measures
        B == beats per measure (constant)
        P == pulses per quarter-note (constant)
        W == beat width in beats per measure (constant)
\endverbatim
 *
 *  For our "b4uacuse" MIDI file, M can be about 100 measures, B is 4,
 *  P can be 192 (but we want to support higher values), and W is 4.
 *  So p = 100 * 4 * 4 * 192 / 4 = 76800 ticks.
 *
 *  Note that 4 * P is a constraint encapsulated by the inline function
 *  pulses_per_measure().
 *
 * \param bpb
 *      The B value in the equation, beats/measure or beats/bar.
 *
 * \param ppqn
 *      The P value in the equation, pulses/qn.
 *
 * \param bw
 *      The W value in the equation, the denominator of the time signature.
 *      If this value is 0, we'll get an arithmetic exception (crash), so we
 *      just return 0 in this case.
 *
 * \param measures
 *      The M value in the equation.  It defaults to 1, in case one desires a
 *      simple "ticks per measure" number.
 *
 * \return
 *      Returns the L value (ticks or pulses) as calculated via the given
 *      equation.  If bw is 0, then 0 is returned.
 */

inline midipulse
measures_to_ticks (int bpb, int ppqn, int bw, int measures = 1)
{
    return (bw > 0) ? midipulse(4 * ppqn * measures * bpb / bw) : 0 ;
}

/**
 *  The inverse of measures_to_ticks.
 *
 * \param B
 *      The B value in the equation, beats/measure or beats/bar.
 *
 * \param P
 *      The P value in the equation, pulses/qn.
 *
 * \param W
 *      The W value in the equation, the denominator of the time signature,
 *      the beat-width.  If this value is 0, we'll get an arithmetic exception
 *      (crash), so we just return 0 in this case.
 *
 * \param p
 *      The p (pulses) value in the equation.
 *
 * \return
 *      Returns the M value (measures or bars) as calculated via the inverse
 *      equation.  If P or B are 0, then 0 is returned.
 */

inline int
ticks_to_measures (midipulse p, int P, int B, int W)
{
    return (P > 0 && B > 0.0) ? (p * W) / (4.0 * P * B) : 0 ;
}

inline int
ticks_to_beats (midipulse p, int P, int B, int W)
{
    return (P > 0 && B > 0.0) ? ((p * W / P / 4 ) % B) : 0 ;
}

/*
 *  Free functions in the seq66 namespace.
 */

extern midipulse pulses_per_substep (midipulse ppqn, int zoom = 1);
extern midipulse pulses_per_pixel (midipulse ppqn, int zoom = 1);
extern midipulse pulses_scaled (midipulse tick, midipulse ppqn, int zoom = 1);
extern midipulse pulse_divide
(
    midipulse numerator,
    midipulse denominator,
    midipulse & remainder
);
extern double wave_func (double angle, wave wavetype);
extern bool extract_port_names
(
    const std::string & fullname,
    std::string & clientname,
    std::string & portname
);
extern std::string extract_bus_name (const std::string & fullname);
extern std::string extract_port_name (const std::string & fullname);
extern std::string extract_a2j_port_name (const std::string & alias);
extern int extract_a2j_bus_id (const std::string & alias);
extern std::string current_date_time ();

}           // namespace seq66

#endif      // SEQ66_CALCULATIONS_HPP

/*
 * calculations.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

