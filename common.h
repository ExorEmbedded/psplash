/*
 *  Contains declarations of the helper functions for Exor dedicated customizations.
 *
 *  Copyright (C) 2014 Exor s.p.a.
 *  Written by: Giovanni Pavoni Exor s.p.a.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_COMMON_H
#define _HAVE_COMMON_H

#define MAXPATHLENGTH           200
#define CMDLINEPATH             "/proc/"

#define ETOP507_VAL     104
#define ETOP507G_VAL    105
#define ECO_VAL         110
#define PLCM07_VAL      111
#define BE15A_VAL       114
#define ETOP7XX_VAL     115
#define PGDXCA16_VAL    117
#define ETOP7XXQ_VAL    118
#define US03KITQ_VAL    119
#define ETOP705_VAL     120
#define WU16_VAL        121
#define US03WU16_VAL    122
#define AUTEC_VAL       123
#define JSMART_VAL      124
#define JSMARTQ_VAL     125
#define JSMARTTTL_VAL   126
#define PGDXCA18_VAL    131


#include <stdio.h>

int gethwcode();
int setbootcounter(unsigned char val);

int sysfs_read(char* pathto, char* fname, char* value, int n);
int sysfs_write(char* pathto, char* fname, char* value);

#endif
