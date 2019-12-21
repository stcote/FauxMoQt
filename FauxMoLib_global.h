#ifndef FAUXMOLIB_GLOBAL_H
#define FAUXMOLIB_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(FAUXMOLIB_LIBRARY)
#  define FAUXMOLIB_EXPORT Q_DECL_EXPORT
#else
#  define FAUXMOLIB_EXPORT Q_DECL_IMPORT
#endif

#endif // FAUXMOLIB_GLOBAL_H
