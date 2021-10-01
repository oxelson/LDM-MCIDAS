


dnl CFORTRAN support for code linked against the McIDAS library:
dnl
AC_DEFUN([UD_CFORTRAN], [dnl
UC_CFORTRAN_PURE
AC_REQUIRE([UC_OS])dnl
case "$OS" in
    aix*)
	AC_MSG_CHECKING(FORTRAN names in McIDAS library)
	libs=$LIBS
	LIBS="$LD_MCIDAS -lxlf ${LIBS-}"
	AC_TRY_LINK(, extern int *uc_,
		    [
			AC_MSG_RESULT(trailing underscores)
			UC_ENSURE(CPPFLAGS, -Dextname)dnl
			UC_ENSURE(FFLAGS, -qextname)dnl
		    ],
			AC_MSG_RESULT(no trailing underscores)
		   )
	LIBS=$libs
	unset libs
	;;
    hp*)
	# Ensure adherence to the trailing-underscore convention.
	UC_ENSURE(CPPFLAGS, -Dextname)dnl
	UC_ENSURE(FFLAGS, +ppu)dnl
	;;
esac
])



dnl FORTRAN library support for code linked against the McIDAS library:
dnl
dnl On a Linux system, the fort77(1) FORTRAN "compiler" requires the
dnl `-static' option in order to successfully link the LDM-McIDAS programs
dnl (otherwise, the symbol `MAIN__', which is referenced by the sharable
dnl f2c library, is unresolved).
dnl
AC_DEFUN([UD_LIB_FORTRAN], [dnl
    AC_REQUIRE([UC_PROG_FC])dnl
    AC_REQUIRE([UC_OS])dnl
    AC_MSG_CHECKING(FORTRAN library)
    case "$OS-$FC" in
	linux-fort77*)
	    UC_ENSURE(LD_FORTRAN, -static)
	    ;;
	*)  LD_FORTRAN=${LD_FORTRAN-}
	    ;;
    esac
    AC_MSG_RESULT($LD_FORTRAN)
    AC_SUBST(LD_FORTRAN)
])


dnl Check for a dynamic-link library.
dnl
AC_DEFUN([UD_LIB_DL], [dnl
    AC_REQUIRE([UC_OS])dnl
    AC_MSG_CHECKING(for dynamic-link library)
    case "${LD_DL-unset}" in
	unset)
	    case "$OS$OS_MAJOR" in
		sunos5*)
		    LD_DL=-ldl
		    ;;
		*)
		    LD_DL=
		    ;;
	    esac
	    ;;
    esac
    AC_SUBST(LD_DL)dnl
    AC_MSG_RESULT($LD_DL)
])
