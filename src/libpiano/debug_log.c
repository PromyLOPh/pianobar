#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "debug_log.h"

static ErrMsgCallback_t gPianoPrintErrMsgCB;

void PianoRegisterErrMsgCallback(ErrMsgCallback_t Arg)
{
   gPianoPrintErrMsgCB = Arg;
}

void PianoPrintErrMsg(const char *format, ...)
{
   va_list fmtargs;

   assert (format != NULL);
   va_start (fmtargs, format);

   if(gPianoPrintErrMsgCB != NULL) {
      gPianoPrintErrMsgCB(format,fmtargs);
   }
   else {
      va_start (fmtargs, format);
      vprintf (format, fmtargs);
      va_end (fmtargs);
   }
}

