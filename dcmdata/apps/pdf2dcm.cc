/*
 *
 *  Copyright (C) 2005-2018, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmdata
 *
 *  Author:  Marco Eichelberg, Pedro Arizpe
 *
 *  Purpose: Encapsulate PDF file into a DICOM file
 *
 */

#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTDIO
#define INCLUDE_CSTRING
#include "dcmtk/ofstd/ofstdinc.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmdata/dcuid.h"       /* for dcmtk version name */
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofdatime.h"
#include "dcmtk/dcmdata/dccodec.h"
#include "dcmtk/dcmdata/dcencdoc.h"

BEGIN_EXTERN_C
#ifdef HAVE_FCNTL_H
#include <fcntl.h>       /* for O_RDONLY */
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>   /* required for sys/stat.h */
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>    /* for stat, fstat */
#endif
END_EXTERN_C

#ifdef WITH_ZLIB
#include <zlib.h>        /* for zlibVersion() */
#endif

#define OFFIS_CONSOLE_APPLICATION "pdf2dcm"

static OFLogger pdf2dcmLogger = OFLog::getLogger("dcmtk.apps." OFFIS_CONSOLE_APPLICATION);

static char rcsid[] = "$dcmtk: " OFFIS_CONSOLE_APPLICATION " v"
OFFIS_DCMTK_VERSION " " OFFIS_DCMTK_RELEASEDATE " $";

int main(int argc, char *argv[])
{
  OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION, "Encapsulate PDF file into DICOM format", rcsid);
  OFCommandLine cmd;
  int errorCode = EXITCODE_NO_ERROR;
  OFCondition result = EC_Normal;
  DcmFileFormat fileformat;
  DcmEncapsulatedDocument encapsulator;

  /*include necessary options*/
  encapsulator.addPDFCommandlineOptions(cmd);
  /* evaluate command line */
  prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);

  if (app.parseCommandLine(cmd, argc, argv)) {
    /* check exclusive options first */
    if (cmd.hasExclusiveOption())
    {
      if (cmd.findOption("--version"))
      {
        app.printHeader(OFTrue /*print host identifier*/);
        COUT << OFendl << "External libraries used: ";
#ifdef WITH_ZLIB
        COUT << OFendl << "- ZLIB, Version " << zlibVersion() << OFendl;
#else
        COUT << " none" << OFendl;
#endif
        return EXITCODE_NO_ERROR;
      }
    }
    encapsulator.parseArguments(app, cmd);
  }
  /* print resource identifier */
  OFLOG_DEBUG(pdf2dcmLogger, rcsid << OFendl);

  /* make sure data dictionary is loaded */
  if (!dcmDataDict.isDictionaryLoaded())
  {
    OFLOG_WARN(pdf2dcmLogger, "no data dictionary loaded, check environment variable: "
      << DCM_DICT_ENVIRONMENT_VARIABLE);
  }
  result = encapsulator.createIdentifiers(pdf2dcmLogger);
  if (result.bad())
  {
    OFLOG_FATAL(pdf2dcmLogger, "There was an error while reading the series data");
    return EXITCODE_INVALID_INPUT_FILE;
  }
  OFLOG_INFO(pdf2dcmLogger, "creating encapsulated PDF object");
  errorCode = encapsulator.insertEncapsulatedDocument(fileformat.getDataset(), pdf2dcmLogger);
  if (errorCode != EXITCODE_NO_ERROR)
  {
    OFLOG_ERROR(pdf2dcmLogger, "unable to create PDF encapsulation to DICOM format");
    return errorCode;
  }
  // now we need to generate an instance number that is guaranteed to be unique within a series.
  result = encapsulator.createHeader(fileformat.getDataset(), pdf2dcmLogger,"","");
  if (result.bad())
  {
    OFLOG_ERROR(pdf2dcmLogger, "unable to create DICOM header: " << result.text());
    return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
  }
  OFLOG_INFO(pdf2dcmLogger, "writing encapsulated PDF object as file " << encapsulator.getOutputFileName());

  OFLOG_INFO(pdf2dcmLogger, "Check if new output transfer syntax is possible");

  DcmXfer opt_oxferSyn(encapsulator.getTransferSyntax());

  fileformat.getDataset()->chooseRepresentation(encapsulator.getTransferSyntax(), NULL);
  if (fileformat.getDataset()->canWriteXfer(encapsulator.getTransferSyntax()))
  {
    OFLOG_INFO(pdf2dcmLogger, "Output transfer syntax " << opt_oxferSyn.getXferName() << " can be written");
  }
  else {
    OFLOG_ERROR(pdf2dcmLogger, "No conversion to transfer syntax " << opt_oxferSyn.getXferName() << " possible!");
    return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
  }
  OFLOG_INFO(pdf2dcmLogger, "Checking for DICOM key overriding");
  result = encapsulator.applyOverrideKeys(fileformat.getDataset());
  if (result.bad())
  {
    OFLOG_ERROR(pdf2dcmLogger, "There was a problem while overriding a key:" << OFendl
      << result.text());
    return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
  }
  OFLOG_INFO(pdf2dcmLogger, "write converted DICOM file with metaheader");
  result = encapsulator.saveFile(fileformat);
  if (result.bad())
  {
    OFLOG_ERROR(pdf2dcmLogger, result.text() << ": writing file: " << encapsulator.getOutputFileName());
    return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
  }

  OFLOG_INFO(pdf2dcmLogger, "PDF encapsulation successful");

  return EXITCODE_NO_ERROR;
}
