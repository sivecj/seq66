# ===========================================================================
# https://www.gnu.org/software/autoconf-archive/ax_have_qt_min.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_HAVE_QT_MIN
#
# DESCRIPTION
#
#   Extended by C. Ahlstrom 2018-03-08 to support rcc.
#
#   Modified by C. Ahlstrom 2019-11-29 to change the macro name to avoid
#   conflicts and to remove the problematic uitools library test.
#
#   Modified by C. Ahlstrom 2020-04-01 to pare down to the minimum modules
#   needed for Seq66: core, gui, and widgets.
#
#   Modified by C. Ahlstrom 2021-07-06 to emit an error message when qmake
#   cannot be found.
#
#   Searches $PATH and queries qmake for Qt include files, libraries and Qt
#   binary utilities. The macro only supports Qt5 or later.
#
#   The following shell variable is set to either "yes" or "no":
#
#     have_qt
#
#   Additionally, the following variables are exported:
#
#     QT_CXXFLAGS
#     QT_LIBS
#     QT_MOC
#     QT_RCC    (new with this version)
#     QT_UIC
#     QT_LRELEASE
#     QT_LUPDATE
#     QT_DIR
#
#   which respectively contain an "-I" flag pointing to the Qt include
#   directory, link flags necessary to link with Qt and X, the full path to
#   the meta object compiler and the user interface compiler both, and
#   finally the variable QTDIR as Qt likes to see it defined.
#
#   Example lines for Makefile.in:
#
#     CXXFLAGS = @QT_CXXFLAGS@
#     MOC      = @QT_MOC@
#
#   After the variables have been set, a trial compile and link is performed
#   to check the correct functioning of the meta object compiler. This test
#   may fail when the different detected elements stem from different
#   releases of the Qt framework. In that case, an error message is emitted
#   and configure stops.
#
#   No common variables such as $LIBS or $CFLAGS are polluted.
#
# LICENSE
#
#   Copyright (c) 2008 Bastiaan Veelo <Bastiaan@Veelo.net>
#   Copyright (c) 2014 Alex Henrie <alexhenrie24@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 15

AU_ALIAS([BNV_HAVE_QT], [AX_HAVE_QT_MIN])
AC_DEFUN([AX_HAVE_QT_MIN],
[
  AC_REQUIRE([AC_PROG_CXX])
  AC_REQUIRE([AC_PATH_X])
  AC_REQUIRE([AC_PATH_XTRA])

  AC_MSG_CHECKING(for Qt)
  # If we have Qt5 or later in the path, we're golden
  ver=`qmake --version | grep -o "Qt version ."`
  if test "$ver" ">" "Qt version 4"; then
    have_qt=yes
    # This pro file dumps qmake's variables, but it only works on Qt 5 or later
    am_have_qt_pro=`mktemp`
    am_have_qt_makefile=`mktemp`
    # http://qt-project.org/doc/qt-5/qmake-variable-reference.html#qt
    cat > $am_have_qt_pro << EOF
qtHaveModule(core):              QT += core
qtHaveModule(gui):               QT += gui
qtHaveModule(widgets):           QT += widgets
percent.target = %
percent.commands = @echo -n "\$(\$(@))\ "
QMAKE_EXTRA_TARGETS += percent
EOF
    qmake $am_have_qt_pro -o $am_have_qt_makefile
    QT_CXXFLAGS=`make -s -f $am_have_qt_makefile CXXFLAGS INCPATH`
    QT_LIBS=`make -s -f $am_have_qt_makefile LIBS`
    rm $am_have_qt_pro $am_have_qt_makefile

    # Look for specific tools in $PATH
    QT_MOC=`which moc`
    QT_RCC=`which rcc`
    QT_UIC=`which uic`
    QT_LRELEASE=`which lrelease`
    QT_LUPDATE=`which lupdate`

    # Get Qt version from qmake
    QT_DIR=`qmake --version | grep -o -E /.+`

    # All variables are defined, report the result
    AC_MSG_RESULT([$have_qt:
    QT_CXXFLAGS=$QT_CXXFLAGS
    QT_DIR=$QT_DIR
    QT_LIBS=$QT_LIBS
    QT_MOC=$QT_MOC
    QT_RCC=$QT_RCC
    QT_UIC=$QT_UIC
    QT_LRELEASE=$QT_LRELEASE
    QT_LUPDATE=$QT_LUPDATE])
  else
    # Qt was not found
    have_qt=no
    QT_CXXFLAGS=
    QT_DIR=
    QT_LIBS=
    QT_MOC=
    QT_RCC=
    QT_UIC=
    QT_LRELEASE=
    QT_LUPDATE=
    AC_MSG_RESULT($have_qt)
    AC_MSG_ERROR(qmake/Qt not found... is it qmake-qt5 on your system?)
  fi
  AC_SUBST(QT_CXXFLAGS)
  AC_SUBST(QT_DIR)
  AC_SUBST(QT_LIBS)
  AC_SUBST(QT_MOC)
  AC_SUBST(QT_RCC)
  AC_SUBST(QT_UIC)
  AC_SUBST(QT_LRELEASE)
  AC_SUBST(QT_LUPDATE)

  #### Being paranoid:
  if test x"$have_qt" = xyes; then
    AC_MSG_CHECKING(correct functioning of Qt installation)
    AC_CACHE_VAL(ax_cv_qt_test_result,
    [
      cat > ax_qt_test.h << EOF
#include <qobject.h>
class Test : public QObject
{
Q_OBJECT
public:
  Test() {}
  ~Test() {}
public slots:
  void receive() {}
signals:
  void send();
};
EOF

      cat > ax_qt_main.$ac_ext << EOF
#include "ax_qt_test.h"
#include <qapplication.h>
int main( int argc, char **argv )
{
  QApplication app( argc, argv );
  Test t;
  QObject::connect( &t, SIGNAL(send()), &t, SLOT(receive()) );
}
EOF

      ax_cv_qt_test_result="failure"
      ax_try_1="$QT_MOC ax_qt_test.h -o moc_ax_qt_test.$ac_ext >/dev/null 2>/dev/null"
      AC_TRY_EVAL(ax_try_1)
      if test x"$ac_status" != x0; then
        echo "$ax_err_1" >&AS_MESSAGE_LOG_FD
        echo "configure: could not run $QT_MOC on:" >&AS_MESSAGE_LOG_FD
        cat ax_qt_test.h >&AS_MESSAGE_LOG_FD
      else
        ax_try_2="$CXX $QT_CXXFLAGS -c $CXXFLAGS -o moc_ax_qt_test.o moc_ax_qt_test.$ac_ext >/dev/null 2>/dev/null"
        AC_TRY_EVAL(ax_try_2)
        if test x"$ac_status" != x0; then
          echo "$ax_err_2" >&AS_MESSAGE_LOG_FD
          echo "configure: could not compile:" >&AS_MESSAGE_LOG_FD
          cat moc_ax_qt_test.$ac_ext >&AS_MESSAGE_LOG_FD
        else
          ax_try_3="$CXX $QT_CXXFLAGS -c $CXXFLAGS -o ax_qt_main.o ax_qt_main.$ac_ext >/dev/null 2>/dev/null"
          AC_TRY_EVAL(ax_try_3)
          if test x"$ac_status" != x0; then
            echo "$ax_err_3" >&AS_MESSAGE_LOG_FD
            echo "configure: could not compile:" >&AS_MESSAGE_LOG_FD
            cat ax_qt_main.$ac_ext >&AS_MESSAGE_LOG_FD
          else
            ax_try_4="$CXX -o ax_qt_main ax_qt_main.o moc_ax_qt_test.o $QT_LIBS $LIBS >/dev/null 2>/dev/null"
            AC_TRY_EVAL(ax_try_4)
            if test x"$ac_status" != x0; then
              echo "$ax_err_4" >&AS_MESSAGE_LOG_FD
            else
              ax_cv_qt_test_result="success"
            fi
          fi
        fi
      fi
    ])dnl AC_CACHE_VAL ax_cv_qt_test_result
    AC_MSG_RESULT([$ax_cv_qt_test_result])
    if test x"$ax_cv_qt_test_result" = "xfailure"; then
      AC_MSG_ERROR([Failed to find matching components of a complete
                  Qt installation. Try using more options,
                  see ./configure --help.])
    fi

    rm -f ax_qt_test.h moc_ax_qt_test.$ac_ext moc_ax_qt_test.o \
          ax_qt_main.$ac_ext ax_qt_main.o ax_qt_main
  fi
])
