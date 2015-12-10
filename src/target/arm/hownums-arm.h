/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#ifndef SYSHOW_H
#define SYSHOW_H

typedef enum {
	HOW_32_to_32 = 0,
    HOW_custom_implementation,
    HOW_not_yet_supported,
    HOW_not_implemented
} Syshow;

static const Syshow syshow_arm[] = {
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_custom_implementation,  /*0..4*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*5..9*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported,  /*10..14*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*15..19*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*20..24*/
HOW_not_yet_supported, HOW_custom_implementation, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*25..29*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*30..34*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*35..39*/
HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*40..44*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*45..49*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*50..54*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_custom_implementation,  /*55..59*/
HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*60..64*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*65..69*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*70..74*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*75..79*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported,  /*80..84*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*85..89*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*90..94*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*95..99*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_32_to_32,  /*100..104*/
HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*105..109*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*110..114*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*115..119*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*120..124*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32,  /*125..129*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*130..134*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*135..139*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*140..144*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*145..149*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*150..154*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*155..159*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32,  /*160..164*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*165..169*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*170..174*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_custom_implementation,  /*175..179*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported,  /*180..184*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*185..189*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*190..194*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*195..199*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*200..204*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*205..209*/
HOW_custom_implementation, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*210..214*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*215..219*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*220..224*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*225..229*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*230..234*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32,  /*235..239*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported,  /*240..244*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*245..249*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*250..254*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*255..259*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*260..264*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*265..269*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*270..274*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*275..279*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*280..284*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*285..289*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*290..294*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*295..299*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*300..304*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*305..309*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*310..314*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*315..319*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*320..324*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*325..329*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*330..334*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*335..339*/
HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*340..344*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*345..349*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*350..354*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*355..359*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*360..364*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*365..369*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*370..374*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_custom_implementation,  /*375..379*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*380..384*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*385..389*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*390..394*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_32_to_32, HOW_not_yet_supported, HOW_not_yet_supported,  /*395..399*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*400..404*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*405..409*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*410..414*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*415..419*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,  /*420..424*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,	/*425..429*/
HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported, HOW_not_yet_supported,	/*430..434*/
HOW_not_yet_supported, HOW_not_yet_supported,																		/*435..436*/
};

#endif /* SYSHOW_H */