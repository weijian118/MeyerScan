#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(MEYERLOGINWIDGET_LIB)
#  define CBLMEYERLOGINWIDGET_EXPORT Q_DECL_EXPORT
# else
#  define CBLMEYERLOGINWIDGET_EXPORT Q_DECL_IMPORT
# endif
#else
# define CBLMEYERLOGINWIDGET_EXPORT
#endif
