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
 * \file          playlistfile.cpp
 *
 *  This module declares/defines the base class for managing the <code>
 *  ~/.seq66rc </code> legacy configuration file or the new <code>
 *  ~/.config/seq66/seq66.rc </code> ("rc") configuration file.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2020-09-19
 * \updates       2021-07-31
 * \license       GNU GPLv2 or above
 *
 *  Here is a skeletal representation of a Seq66 playlist file:
 *
 \verbatim
        [playlist]
        0                       # playlist number, a MIDI value (0 to 127)
        "Downtempo"             # playlist name, for display/selection
        /home/user/midifiles/   # directory where the songs are stored
        10 file1.mid            # MIDI value and file's base-name
        11 file2.midi
        12 file3.midi           # . . .
\endverbatim
 *
 *  See the file data/sample.playlist for a more up-to-date example and
 *  explanation.
 */

#include "cfg/playlistfile.hpp"         /* seq66::playlistfile class        */
#include "cfg/settings.hpp"             /* seq66::rc()                      */
#include "play/playlist.hpp"            /* seq66::playlist class            */
#include "util/calculations.hpp"        /* seq66::current_date_time()       */
#include "util/filefunctions.hpp"       /* functions for file-names         */
#include "util/strfunctions.hpp"        /* strip_quotes()                   */

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{

static const int s_playlist_file_version = 1;

/**
 *  Principal constructor.
 *
 * \param p
 *      Provides the performer object that will interface between this module
 *      and the rest of the application.
 *
 * \param name
 *      Provides the name of the options file; this is usually a full path
 *      file-specification.
 *
 * \param rcs
 *      Provides a reference to an "rc" settings object to hold current options
 *      and modified options.
 *
 * \param show_on_stdout
 *      If true (the default is false), then the list/song information is
 *      written to stdout, to help with debugging.
 */

playlistfile::playlistfile
(
    const std::string & filename,
    playlist & pl,
    rcsettings & rcs,
    bool show_on_stdout
) :
    configfile          (filename, rcs),
    m_play_list         (pl),
    m_show_on_stdout    (show_on_stdout)
{
    version(s_playlist_file_version);
}

/**
 *  This destructor is a rote do-nothing destructor.
 */

playlistfile::~playlistfile ()
{
    // No code needed
}

/**
 *  Helper function for error-handling.  It assembles a message and then
 *  passes it to append_error_message().
 *
 * \param additional
 *      Additional context information to help in finding the error.
 *
 * \return
 *      Always returns false.
 */

bool
playlistfile::set_error_message (const std::string & additional)
{
    std::string msg = "Play-list file";
    if (! additional.empty())
    {
        msg += ": ";
        msg += additional;
    }
    errprint(msg.c_str());
    append_error_message(msg);
    return false;
}

/**
 *  Opens the current play-list file and optionally verifies it.
 *
 * \param verify_it
 *      If true (the default), call verify() to make sure the playlist is
 *      sane.  The verification is weak, rather than the strong option which
 *      is available.
 *
 * \return
 *      Returns true if the file was parseable and verifiable.  The caller
 *      (especially the performer) may want to do a "clear all".
 */

bool
playlistfile::open (bool verify_it)
{
    bool result = parse();
    if (result)
    {
        if (verify_it)
        {
            if (m_show_on_stdout)
                printf("Verifying playlist %s\n", name().c_str());

            result = play_list().verify();                  /* weak verify  */
        }
    }
    play_list().mode(result);
    return result;
}

/**
 *  Parses the ~/.config/seq66/file.playlistfile file.
 *
 * The next_section() function is like line-after, but scans from the
 * current line in the file.  Necessary here because all the sections
 * have the same name.  After detecting the "[playlist]" section, the
 * following items need to be obtained:
 *
 *      -   Playlist number.  This number is used as the key value for
 *          the playlist. It can be any MIDI value (0 to 127), and the order
 *          of the playlists is based on this number, and selectable via MIDI
 *          control with this number.
 *      -   Playlist name.  A human-readable string describing the
 *          nick-name for the playlist.  This is an alternate way to
 *          look up the playlist.
 *      -   Song directory name.  The directory where the songs are
 *          stored.  If this name is empty, then the song file-names
 *          need to include the individual directories for each file.
 *          But even if not empty, the play-list directory is not used if
 *          the song file-name includes a path, as indicated by "/" or "\".
 *      -   Song file-name, or path to the song file-name.
 *
 *  Note that the call to next_section() already gets to the next line of
 *  data, which should be the index number of the playlist.
 *
 *  We need to get the song's MIDI control number and it's directory name.
 *  Make sure the directory name is canonical and clean.  The existence of the
 *  file should be validated later.  Also determine if the song file-name
 *  already has a directory before using the play-list's directory.
 *
 *  Consider catching file exceptions!
 *
 * \return
 *      Returns true if the file was able to be opened for reading.
 *      Currently, there is no indication if the parsing actually succeeded.
 */

bool
playlistfile::parse ()
{
    if (is_empty_string(name()))
        return false;

    std::ifstream file(name(), std::ios::in | std::ios::ate);
    bool result = file.is_open();
    if (result)
    {
        file_message("Reading playlist", name());
        file.seekg(0, std::ios::beg);                   /* seek to start    */
        play_list().clear();
        (void) parse_version(file);

        std::string temp = parse_comments(file);
        if (! temp.empty())
            play_list().comments_block().set(temp);

        /*
         * [playlist-options]
         */

        std::string tag = "[playlist-options]";
        if (file_version_number() < s_playlist_file_version)
        {
            if (line_after(file, tag))
            {
                int unmute = 0;
                sscanf(scanline(), "%d", &unmute);
                play_list().unmute_set_now(unmute != 0);
                if (next_data_line(file))
                {
                    sscanf(scanline(), "%d", &unmute);
                    play_list().deep_verify(unmute != 0);
                }
                else
                    play_list().deep_verify(false);
            }
        }
        else
        {
            bool flag = get_boolean(file, tag, "unmute-new-song");
            play_list().unmute_set_now(flag);
            flag = get_boolean(file, tag, "deep-verify");
            play_list().deep_verify(flag);
        }

        int listcount = 0;
        bool have_section = line_after(file, "[playlist]");
        if (! have_section)
        {
            result = set_error_message("empty/missing [playlist] section");
        }
        while (have_section)
        {
            int listnumber = -1;
            int songcount = 0;
            playlist::play_list_t plist;            /* current playlist     */
            if (file_version_number() < s_playlist_file_version)
                sscanf(scanline(), "%d", &listnumber);  /* playlist number      */
            else
                sscanf(scanline(), "number = %d", &listnumber);

            if (m_show_on_stdout)
                printf("Processing playlist #%d\n", listnumber);

            if (next_data_line(file))
            {
                std::string listline = line();
                playlist::song_list slist;
                if (file_version_number() < s_playlist_file_version)
                {
                    listline = strip_quotes(listline);
                }
                else
                {
                    listline = extract_variable(line(), "name");
                }

                plist.ls_list_name = listline;
                if (m_show_on_stdout)
                    printf("Playlist name '%s'\n", listline.c_str());

                if (next_data_line(file))
                {
                    listline = line();
                    if (file_version_number() < s_playlist_file_version)
                    {
                        listline = strip_quotes(listline);
                    }
                    else
                    {
                        listline = extract_variable(line(), "directory");
                    }
                    plist.ls_file_directory = clean_path(listline);
                    slist.clear();
                    if (m_show_on_stdout)
                        printf("Playlist directory '%s'\n", listline.c_str());

                    while (next_data_line(file))
                    {
                        int songnumber = -1;
                        std::string fname;
                        result = scan_song_file(songnumber, fname);
                        fname = strip_quotes(fname);
                        if (result)
                        {
                            playlist::song_spec_t sinfo;
                            sinfo.ss_index = songcount;
                            sinfo.ss_midi_number = songnumber;
                            if (name_has_path(fname))
                            {
                                std::string path;
                                std::string filebase;
                                filename_split(fname, path, filebase);
                                sinfo.ss_song_directory = path;
                                sinfo.ss_embedded_song_directory = true;
                                sinfo.ss_filename = filebase;
                            }
                            else if (! fname.empty())
                            {
                                sinfo.ss_song_directory = plist.ls_file_directory;
                                sinfo.ss_embedded_song_directory = false;
                                sinfo.ss_filename = fname;
                                (void) play_list().add_song(slist, sinfo);
                                ++songcount;
                            }
                        }
                        else
                        {
                            std::string msg = "scanning song file '";
                            msg += fname;
                            msg += "' failed";
                            result = set_error_message(msg);
                            break;
                        }
                    }

                    /*
                     * Need to deal with a false result?  It's not really a
                     * fatal error to have an empty song list.
                     */

                    plist.ls_index = listcount;         /* ordinal      */
                    plist.ls_midi_number = listnumber;  /* MIDI mapping */
                    plist.ls_song_count = songcount;
                    plist.ls_song_list = slist;         /* copy temp    */
                    result = play_list().add_list(plist);
                }
                else
                {
                    std::string msg = "no list directory in playlist #" +
                        std::to_string(listnumber);

                    result = set_error_message(msg);
                    break;
                }
            }
            else
            {
                std::string msg = "no data in playlist #" +
                    std::to_string(listnumber);

                result = set_error_message(msg);
                break;
            }
            ++listcount;
             have_section = next_section(file, "[playlist]");
        }
        file.close();                           /* done with playlist file  */
    }
    else if (rc().playlist_active() && ! name().empty())
    {
        std::string msg = "Open failed: " + name();
        result = set_error_message(msg);
    }
    (void) play_list().reset_list(0, ! result); /* reset, not clear, if ok  */
    return result;
}

/**
 *  Encapsulates some groty code for the parse() function.  It assumes that
 *  next_data_line() has retrieved a file-name line for a song.
 *
 * \param [out] song_number
 *      Holds the song number that was retrieved.  Use it only if not equal
 *      to -1 and if this function returns true.
 *
 * \param [out] song_file
 *      Holds the song file-name that was retrieved.  Use it only if not
 *      empty and if this function returns true.
 *
 * \return
 *      Returns true if this function succeeded.  If false, an error message is
 *      set up.
 */

bool
playlistfile::scan_song_file (int & song_number, std::string & song_file)
{
    bool result = false;
    int songnumber = -1;
    const char * dirname = &m_line[0];
    int sscount = sscanf(scanline(), "%d", &songnumber);
    if (sscount == EOF || sscount == 0)
    {
        song_number = -1;                                   /* side-effect  */
        song_file.clear();                                  /* side-effect  */
        result = set_error_message("song number missing");
    }
    else
    {
        while (! std::isspace(*dirname))
        {
            if (*dirname == 0)
                break;

            ++dirname;
        }
        while (std::isspace(*dirname))
        {
            if (*dirname == 0)
                break;

            ++dirname;
        }
        bool gotit = std::isalnum(*dirname) || std::ispunct(*dirname);
        if (gotit)
        {
            song_number = songnumber;                       /* side-effect  */
            song_file = dirname;                            /* side-effect  */
            result = true;
        }
        else
        {
            song_number = -1;                               /* side-effect  */
            song_file.clear();                              /* side-effect  */
            result = set_error_message("song file-path missing");
        }
    }
    return result;
}

/**
 *  Writes the play-list to the file whose name is returned by the name()
 *  function.  If the name is empty, we don't try to open it; that truncates
 *  the file!
 *
 *  Consider catching file exceptions!
 *
 * \return
 *      Returns true if the write operations all succeeded.
 */

bool
playlistfile::write ()
{
    std::ofstream file(name(), std::ios::out | std::ios::trunc);
    bool result = ! name().empty() && file.is_open();
    if (result)
    {
        file_message("Writing playlist", name());
    }
    else
    {
        file_error("Write open fail", name());
        return result;
    }
    write_date(file, "playlist");
    file <<
        "# This file holds multiple playlists. It consists of 1 or more\n"
        "# [playlist] sections.  Each has a user-specified number for sorting\n"
        "# and MIDI control, ranging from 0 to 127. Next comes a quoted display\n"
        "# name for this list, followed by the quoted name of the song folder\n"
        "# using the UNIX-style separator ('/'). It should be accessible from\n"
        "# wherever Seq66 is run.\n"
        "#\n"
        "# Next is a list of tunes, each starting with a MIDI control number\n"
        "# and the quoted name of the MIDI file, sorted by the control number.\n"
        "# They can be simple 'base.midi' file-names; the playlist directory\n"
        "# is prepended to access the song. If the file-name has a path, that\n"
        "# will be used.\n"
        ;
    write_seq66_header(file, "playlist", version());

    /*
     * [comments]
     */

    std::string c = play_list().comments_block().text();
    write_comment(file, c);

    /*
     * [playlist-options]
     */

    file << "\n"
        << "[playlist-options]\n" << "\n"
        ;
    write_boolean(file, "unmute-new-song", play_list().unmute_set_now());
    write_boolean(file, "deep-verify", play_list().deep_verify());
    file << "\n"
       "# First provide the playlist settings, then its default storage folder\n"
       "# and then list each tune with its control number. The playlist\n"
       "# number is arbitrary but unique. 0 to 127 recommended for use with\n"
       "# the MIDI playlist control. Similar for the tune numbers. Each \n"
       "# tune can include a path; it overrides the base directory.\n"
       ;

    /*
     * [playlist] sections
     */

    bool is_empty = true;
    for (const auto & plpair : play_list().play_list_map())
    {
        const playlist::play_list_t & pl = plpair.second;
        std::string listname = add_quotes(pl.ls_list_name);
        std::string dirname = add_quotes(pl.ls_file_directory);
        file << "\n"
            "[playlist]\n\n"
            << "number = " << pl.ls_midi_number << "\n"
            << "name = " << listname << "\n"
            << "directory = " << dirname << "\n\n"
            ;

        /*
         * For each song, write the MIDI control number, followed only by
         * the song's file-name, which could include the path-name.
         */

        const playlist::song_list & sl = pl.ls_song_list;
        for (const auto & sc : sl)
        {
            const playlist::song_spec_t & s = sc.second;
            std::string fname = s.ss_filename;
            fname = add_quotes(fname);
            file << s.ss_midi_number << " " << fname << "\n";
        }
        is_empty = false;
    }
    if (is_empty)
    {
        file
        << "\n[playlist]\n\n"
           "# This is a NON-FUNCTIONAL playlist SAMPLE. Please see one of the\n"
           "# sample playlist files shipping with Seq66.\n\n"
            ;
    }
    file
        << "\n"
        << "# End of " << name() << "\n#\n"
        << "# vim: sw=4 ts=4 wm=4 et ft=dosini\n"   /* ft for nice colors */
        ;

    file.close();
    return true;
}

/*
 * Free functions for handling play-lists and files.
 */

/**
 *  Reads the play-list data into a playlist object.
 *
 * \param source
 *      Provides the full path file-specification for the play-list file to be
 *      opened.
 *
 * \param show_on_stdout
 *      If true (the default is false), the playlist is opened to show
 *      song selections on stdout.  This is useful for trouble-shooting or for
 *      making the CLI version of Sequencer64 easier to follow when running.
 *
 * \return
 *      Returns true if the playlist object was able to be created. If the
 *      file-name is not empty, this also means that it was opened, and the
 *      play-list read.  If false is returned, then the previous playlist, if
 *      any, still exists, but is marked as inactive.
 */

bool
open_playlist
(
    playlist & pl,
    const std::string & source,
    bool show_on_stdout
)
{
    bool result = ! is_missing_string(source);  /* empty, "", or "?"        */
    if (result)
    {
        playlistfile plf(source, pl, rc(), show_on_stdout);
        result = plf.open(true);            /* parse and file verify    */
        if (result)
        {
            // Anything worth doing?
        }
        else if (rc().playlist_active())
        {
            std::string msg = "Open failed: ";
            msg += source;
            (void) error_message(msg);
        }
    }
    else
    {
        file_error("Play-list file to open", "none");
        pl.mode(false);
    }
    return result;
}

/**
 *  Writes the play-list, whether it is active or not, as long as it exists.
 *  Saves a playlist to file.  Used in clinsmanager and in performer.
 *
 * \param pl
 *      Provides a reference to the playlist.  The caller is responsible for
 *      making sure this parameter is valid.
 *
 * \param destfile
 *      Provides the full path file-specification for the play-list file to be
 *      saved.  If empty, the file-name with which the play-list was created
 *      is used.
 *
 * \return
 *      Returns true if the write operation succeeded.
 *
 */

bool
save_playlist
(
    playlist & pl,
    const std::string & destfile
)
{
    std::string destination = destfile.empty() ? pl.file_name() : destfile;
    bool result = ! is_missing_string(destination);     /* empty, "", "?"   */
    if (result)
    {
        playlistfile plf(destination, pl, rc(), false); /* false --> quiet  */

        /*
         * Redundant: file_message("Play-list save", destination);
         */

        pl.file_name(destination);
        result = plf.write();
        if (! result)
        {
            file_error("Play-list write failed", destination);
        }
    }
    else
    {
        file_error("Play-list file to save", "none");
    }
    return result;
}

/**
 *  This function reads the source playlist file and then saves it to the new
 *  location.
 *
 *  \param [inout] pl
 *      Provides the playlist object.
 *
 *  \param source
 *      Provides the input file name from which the playlist will be filled.
 *
 *  \param destination
 *      Provides the directory to which the play-list file is to be saved.
 *
 * \return
 *      Returns true if the operation succeeded.
 */

bool
save_playlist
(
    playlist & pl,
    const std::string & source,
    const std::string & destination
)
{
    bool result = ! source.empty() && ! destination.empty();
    if (result)
    {
        playlistfile plf(source, pl, rc(), false);  /* false --> quiet      */
        result = plf.open(false);                   /* parse, no verify     */
        if (result)
        {
            result = save_playlist(pl, destination);
        }
        else
        {
            file_error("Play-list open failed", source);
        }
    }
    else
    {
        file_error("Play-list file to save", "none");
    }
    return result;
}

/**
 *  This function uses the playlist to copy all of the MIDI files noted in the
 *  source playlist file.
 *
 *  \param [in] plp
 *      Provides a pointer to the playlist, which should have been filled with
 *      playlist data.
 *
 *  \param source
 *      Simply provides the input file name for information only.
 *
 *  \param destination
 *      Provides the directory to which the play-list file is to be saved.
 *
 * \return
 *      Returns true if the operation succeeded.
 */

bool
copy_playlist_songs
(
    playlist & pl,
    const std::string & source,
    const std::string & destination
)
{
    bool result = ! source.empty() && ! destination.empty();
    if (result)
    {
        std::string msg = source + " --> " + destination;
        file_message("Play-list copy", msg);
        result = pl.copy_songs(destination);
        if (! result)
            file_error("Copy failed", destination);
    }
    else
    {
        file_error("Play-list file directories", "<empty>");
    }
    return result;
}

}           // namespace seq66

/*
 * playlistfile.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

