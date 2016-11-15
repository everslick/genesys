/*
    This file is part of Genesys.

    Genesys is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Genesys is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

#ifndef _I18N_H_
#define _I18N_H_

// definition for DST start and end: i,j,k,l
// -----------------------------------------
// i: month (1=jan, 2=feb, ...)
// j: hour when to switch (local time)
// k: nth occurance in month (1=fist, 2=second, ... -1=last, -2=penultimate, ...)
// l: day of week (0=sun, 1=mon, 2=tue, ...)

// default (UK)
#define I18N_DATE_FORMAT         "%i/%02i/%02i"
#define I18N_DATE_ORDER          year, month, day
#define I18N_TZ_OFFSET           0
#define I18N_DST_START    3,2,-1,0 // march,   2:00, last, sunday
#define I18N_DST_END     10,2,-1,0 // october, 2:00, last, sunday
#define I18N_FLOAT_COMMA         '.'

// undefined
#ifndef I18N_COUNTRY_CODE
#warning "undefined I18N_COUNTRY_CODE, using default values"

// germany
#elif I18N_COUNTRY_CODE == DE
#define I18N_DATE_FORMAT         "%i.%i.%i"
#define I18N_DATE_ORDER          day, month, year
#define I18N_TZ_OFFSET           60
#define I18N_FLOAT_COMMA         ','

// united states of america
#elif I18N_COUNTRY_CODE == US
#define I18N_DATE_FORMAT         "%02i/%02i/%i"
#define I18N_DATE_ORDER          month, day, year
#define I18N_TZ_OFFSET        -300
#define I18N_DST_START     3,2,2,0 // march,    1:00, second, sunday
#define I18N_DST_END      11,2,1,0 // november, 1:00, first,  sunday
#define I18N_FLOAT_COMMA         '.'

// unknown
#else
#warning "unknown I18N_COUNTRY_CODE, using default values"
#endif

#endif // _I18N_H_
