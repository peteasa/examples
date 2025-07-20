/*
  e_monitor.c

  Copyright (c) 2025 Peter Saunderson <peteasa@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program, see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <e-lib.h>
#include <stdlib.h>

int main(void)
{

    /* Create delay */
	int i;
	for (i = 0; i < 100000000; i++)
		__asm__ __volatile__ ("nop" ::: "memory");

    return EXIT_SUCCESS;
}

