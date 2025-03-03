#if ! defined SEQ66_QSEQROLL_HPP
#define SEQ66_QSEQROLL_HPP

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
 * \file          qseqroll.hpp
 *
 *  This module declares/defines the base class for drawing on the piano
 *  roll of the patterns editor.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2018-01-01
 * \updates       2021-07-28
 * \license       GNU GPLv2 or above
 *
 *  We are currently moving toward making this class a base class.
 *
 *  User jean-emmanual added support for disabling the following of the
 *  progress bar during playback.  See the qseqbase::m_progress_follow member.
 */

#include <QWidget>

#include "cfg/scales.hpp"               /* seq66::scales enum class         */
#include "play/sequence.hpp"            /* sequence::editmode mode          */
#include "qseqbase.hpp"                 /* seq66::qseqbase mixin class      */

/*
 * Forward references
 */

class QMessageBox;
class QPixmap;
class QTimer;

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{
    class performer;
    class qseqeditframe64;
    class qseqkeys;

/**
 * The MIDI note grid in the sequence editor
 */

class qseqroll final : public QWidget, public qseqbase
{
    friend class qseqeditframe64;

    Q_OBJECT

public:

    qseqroll
    (
        performer & perf,
        seq::pointer seqp,
        qseqeditframe64 * parent,
        qseqkeys * seqkeys_wid,
        int zoom                    = c_default_zoom,
        int snap                    = c_default_snap,
        sequence::editmode mode     = sequence::editmode::note,
        int unit_height             =  1,
        int total_height            =  1
    );

    virtual ~qseqroll ();

    void follow_progress ();

    bool v_zoom_in ();
    bool v_zoom_out ();
    bool reset_v_zoom ();
    int note_height () const;

    virtual bool zoom_in () override;
    virtual bool zoom_out () override;
    virtual bool reset_zoom () override;

    const Color & backseq_color () const
    {
        return m_backseq_color;
    }

private:

    virtual void scroll_offset (int v) override;

    virtual int scroll_offset () const override
    {
        return qseqbase::scroll_offset();
    }

    void set_redraw ();

    bool is_drum_mode () const
    {
        return m_edit_mode == sequence::editmode::drum;
    }

    int get_note_length () const
    {
        return m_note_length;
    }

    void set_note_length (int len)
    {
        m_note_length = len;
    }

    void set_chord (int chord);
    void set_key (int key);
    void set_scale (int scale);
    void set_background_sequence (bool state, int seq);
    void analyze_seq_notes ();
    int note_off_length () const;
    bool add_painted_note (midipulse tick, int note);
    bool zoom_key_press (bool shifted, int key);
    bool movement_key_press (int key);

private:        // overrides for painting, mouse/keyboard events, & size hints

    virtual void paintEvent (QPaintEvent *) override;
    virtual void resizeEvent (QResizeEvent *) override;
    virtual void mousePressEvent (QMouseEvent *) override;
    virtual void mouseReleaseEvent (QMouseEvent *) override;
    virtual void mouseMoveEvent (QMouseEvent *) override;
    virtual void keyPressEvent (QKeyEvent *) override;
    virtual void keyReleaseEvent (QKeyEvent *) override;
    virtual QSize sizeHint () const override;

private:

    virtual void update_midi_buttons () override
    {
        // no code needed, no buttons or statuses to update at this time
    }

private:

#if defined USE_GROW_SELECTED_NOTES_FUNCTION
    void grow_selected_notes (int dx);
#endif

    void move_selected_notes (int dx, int dy);
    void snap_y (int & y);
    void set_adding (bool a_adding);
    void start_paste();
    void draw_grid (QPainter & painter, const QRect & r);
    void draw_notes (QPainter & painter, const QRect & r, bool background);
    void draw_drum_notes (QPainter & painter, const QRect & r, bool background);
    void draw_drum_note (QPainter & painter, int x, int y);
    void call_draw_notes (QPainter & painter, const QRect & view);

private:

    /**
     *  Used for showing the estimated scale/key upon a Ctrl-K in the qseqroll.
     */

    QMessageBox * m_analysis_msg;

    /**
     *  The color (from the palette) for the background sequence.
     */

    const Color m_backseq_color;

    /**
     *  Holds a pointer to the qseqkeys pane that is associated with the
     *  qseqroll piano roll.
     */

    qseqkeys * m_seqkeys_wid;

    /**
     *  Screen update timer.
     */

    QTimer * m_timer;

    /**
     *  The width, in pixels, of the progress-bar/playhead.  Usually 1 or 2
     *  pixels.
     */

    int m_progbar_width;

    /**
     *  Indicates the musical scale in force for this sequence.
     */

    scales m_scale;

    /**
     *  A position value.  Need to clarify what exactly this member is used
     *  for.
     */

    int m_pos;

    /**
     *  Indicates either that chord support is disabled (0), or a particular
     *  chord is to be created when inserting notes.
     */

    int m_chord;

    /**
     *  The current musical key selected.
     */

    int m_key;

    /**
     *  Holds the note length in force for this sequence.  Used in the
     *  seq66seqroll module only.
     */

    int m_note_length;

    /**
     *  Provides the number of ticks to shave off of the end of painted notes.
     *  Also used when the user attempts to shrink a note to zero (or less
     *  than zero) length.
     */

    const midipulse m_note_off_margin;

    /**
     *  Holds the value of the musical background sequence that is shown in
     *  cyan (formerly grey) on the background of the piano roll.
     */

    int m_background_sequence;

    /**
     *  Set to true if the drawing of the background sequence is to be done.
     */

    bool m_drawing_background_seq;

    /**
     *  The current status/event selected in the seqedit.  Not used in seqroll
     *  at present.
     */

    midibyte m_status;

    /**
     *  The current MIDI control value selected in the seqedit.  Not used in
     *  seqroll at present.
     */

    midibyte m_cc;

    /**
     *  Indicates the edit mode, note versus drum.
     */

    sequence::editmode m_edit_mode;

    /**
     *  Indicates to draw the whole grid.
     */

    bool m_draw_whole_grid;

    /**
     *  The starting time, in ticks, of the current frame.
     */

    mutable midipulse m_t0;

    /**
     *  The ending time, in ticks, of the current frame.
     */

    mutable midipulse m_t1;

    /**
     *  The width of a frame in ticks.
     */

    mutable midipulse m_frame_ticks;

    int m_note_x;               // note drawing variables
    int m_note_width;
    int m_note_y;
    int m_keypadding_x;
    bool m_v_zooming;

    /**
     *  Hold the note value first grabbed when starting a move.
     */

    int m_last_base_note;

signals:

public slots:

    void conditional_update ();
    void update_edit_mode (sequence::editmode mode);

};          // class qseqroll

}           // namespace seq66

#endif      // SEQ66_QSEQROLL_HPP

/*
 * qseqroll.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

