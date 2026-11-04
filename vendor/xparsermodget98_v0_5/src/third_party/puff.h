/* puff.h
  Copyright (C) 2002, 2003 Mark Adler, all rights reserved
  version 1.7, 3 Mar 2002

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */


/*
 * See puff.c for purpose and usage.
 */
/* Legacy puff() intentionally not exported in model_runtime89.
 * Blank3D FontCore owns the process-wide puff symbol; this vendor uses
 * puff_ex() to avoid duplicate-definition collisions. */

/* extension for MSZIP preset dictionary support */
int puff_ex(unsigned char *dest, unsigned long *destlen, unsigned char *source, unsigned long *sourcelen, unsigned long outcnt0);
