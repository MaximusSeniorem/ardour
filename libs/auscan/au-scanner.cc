/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <string>

#include "pbd/error.h"
#include "pbd/transmitter.h"
#include "pbd/receiver.h"
#include "pbd/pbd.h"

#include "CAAudioUnit.h"
#include "CAAUParameter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#ifdef WITH_CARBON
#include <Carbon/Carbon.h>
#endif

#ifdef COREAUDIO105
#define ArdourComponent Component
#define ArdourDescription ComponentDescription
#define ArdourFindNext FindNextComponent
#else
#define ArdourComponent AudioComponent
#define ArdourDescription AudioComponentDescription
#define ArdourFindNext AudioComponentFindNext
#endif

#include "pbd/i18n.h"

using namespace std;

#include "../ardour/filesystem_paths.cc"

using namespace PBD;

class LogReceiver : public Receiver
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		switch (chn) {
			case Transmitter::Debug:
				/* ignore */
				break;
			case Transmitter::Info:
				prefix = "[Info]: ";
				break;
			case Transmitter::Warning:
				prefix = "[WARNING]: ";
				break;
			case Transmitter::Error:
				prefix = "[ERROR]: ";
				break;
			case Transmitter::Fatal:
				prefix = "[FATAL]: ";
				break;
			case Transmitter::Throw:
				abort ();
		}

		std::cout << prefix << str << std::endl;

		if (chn == Transmitter::Fatal) {
			::exit (EXIT_FAILURE);
		}
	}
};

LogReceiver log_receiver;

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf ("ardour-au-scanner - load and index AudioUnit plugins.\n\n");
	printf ("Usage: ardour-au-scanner [ OPTIONS ] <TYPE> <SUBT> <MANU>\n\n");
	printf ("Options:\n\
  -f, --force          Force update of cache file\n\
  -h, --help           Display this help and exit\n\
  -q, --quiet          Hide usual output, only print errors\n\
  -v, --verbose        Give verbose output (unless quiet)\n\
  -V, --version        Print version information and exit\n\
\n");

	printf ("\n\
This tool ...\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
		"Website: <http://ardour.org/>\n");

	::exit (EXIT_SUCCESS);
}


int
main (int argc, char **argv)
{
	bool print_log = true;
	bool force = false;
	bool verbose = false;

	const char* optstring = "fhqvV";

	/* clang-format off */
	const struct option longopts[] = {
		{ "force",           no_argument,       0, 'f' },
		{ "help",            no_argument,       0, 'h' },
		{ "quiet",           no_argument,       0, 'q' },
		{ "verbose",         no_argument,       0, 'v' },
		{ "version",         no_argument,       0, 'V' },
	};
	/* clang-format on */

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv, optstring, longopts, (int*)0))) {
		switch (c) {
			case 'V':
				printf ("ardour-au-scanner version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2021 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'f':
				force = true;
				break;

			case 'h':
				usage ();
				break;

			case 'q':
				print_log = false;
				break;

			case 'v':
				verbose = true;
				break;

			default:
				std::cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind + 3 != argc) {
		std::cerr << "Error: Missing parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}


	PBD::init();

	if (print_log) {
		log_receiver.listen_to (info);
		log_receiver.listen_to (warning);
		log_receiver.listen_to (error);
		log_receiver.listen_to (fatal);
	} else {
		verbose = false;
	}

	bool err = false;

	CFStringRef s_type = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind++], kCFStringEncodingUTF8);
	CFStringRef s_subt = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind++], kCFStringEncodingUTF8);
	CFStringRef s_manu = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind], kCFStringEncodingUTF8);

	OSType type = UTGetOSTypeFromString (s_type);
	OSType subt = UTGetOSTypeFromString (s_subt);
	OSType manu = UTGetOSTypeFromString (s_manu);

	CAComponentDescription desc (type, subt, manu);

	ArdourComponent comp = ArdourFindNext (NULL, &desc);

	if (comp == NULL) {
		error << ("AU was not foundn") << endmsg;
		err = true;
	}

	while (comp != NULL) {
		CAComponentDescription temp;
#ifdef COREAUDIO105
		GetComponentInfo (comp, &temp, NULL, NULL, NULL);
#else
		AudioComponentGetDescription (comp, &temp);
#endif
		info << ("Component loaded") << endmsg;

		assert (temp.componentType == desc.componentType);
		assert (temp.componentSubType == desc.componentSubType);
		assert (temp.componentManufacturer == desc.componentManufacturer);

		CFStringRef itemName = NULL;

		{
			if (itemName != NULL) {
				CFRelease(itemName);
			}

			CFStringRef compTypeString         = UTCreateStringForOSType(temp.componentType);
			CFStringRef compSubTypeString      = UTCreateStringForOSType(temp.componentSubType);
			CFStringRef compManufacturerString = UTCreateStringForOSType(temp.componentManufacturer);

			itemName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ - %@ - %@"), compTypeString, compManufacturerString, compSubTypeString);

			//au_crashlog(string_compose("Scanning ID: %1", CFStringRefToStdString(itemName)));
			if (compTypeString != NULL) {
				CFRelease(compTypeString);
			}
			if (compSubTypeString != NULL) {
				CFRelease(compSubTypeString);
			}
			if (compManufacturerString != NULL) {
				CFRelease(compManufacturerString);
			}
		}

#if 0
		if (is_blacklisted(CFStringRefToStdString(itemName))) {
			if (itemName != NULL) {
				CFRelease(itemName);
				itemName = NULL;
			}

			info << string_compose (_("Skipped blacklisted AU plugin %1 "), CFStringRefToStdString(itemName)) << endmsg;
			comp = ArdourFindNext (comp, &desc);
			continue;
		}
#endif

		//au_unblacklist(CFStringRefToStdString(itemName));
		//au_crashlog("Success.");
	
		comp = ArdourFindNext (comp, &desc);
		assert (comp == NULL);
		if (itemName != NULL) {
			CFRelease(itemName); itemName = NULL;
		}
	}

	CFRelease (s_type);
	CFRelease (s_subt);
	CFRelease (s_manu);

	PBD::cleanup();

	if (err) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
