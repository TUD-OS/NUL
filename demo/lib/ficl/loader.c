/*-
 * Copyright (c) 2000 Daniel Capo Sobral
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*******************************************************************
** l o a d e r . c
** Additional FICL words designed for FreeBSD's loader
** 
*******************************************************************/

#include <stdlib.h>
#include <string.h>
#include "ficl.h"

/*		FreeBSD's loader interaction words and extras
 *
 * 		ccall       ( [[...[p10] p9] ... p1] n addr -- result )
 * 		.#	    ( value -- )
 */

void
ficlCcall(FICL_VM *pVM)
{
	int (*func)(int, ...);
	int result, p[10];
	int nparam, i;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 0);
#endif

	func = stackPopPtr(pVM->pStack);
	nparam = stackPopINT(pVM->pStack);

#if FICL_ROBUST > 1
	vmCheckStack(pVM, nparam, 1);
#endif

	for (i = 0; i < nparam; i++)
		p[i] = stackPopINT(pVM->pStack);

	result = func(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8],
	    p[9]);

	stackPushINT(pVM->pStack, result);

	return;
}

/*
** Retrieves free space remaining on the dictionary
*/

static void freeHeap(FICL_VM *pVM)
{
    stackPushINT(pVM->pStack, dictCellsAvail(ficlGetDict(pVM->pSys)));
}


/******************* Increase dictionary size on-demand ******************/
 
static void ficlDictThreshold(FICL_VM *pVM)
{
    stackPushPtr(pVM->pStack, &dictThreshold);
}
 
static void ficlDictIncrease(FICL_VM *pVM)
{
    stackPushPtr(pVM->pStack, &dictIncrease);
}


/**************************************************************************
                        f i c l C o m p i l e P l a t f o r m
** Build FreeBSD platform extensions into the system dictionary
**************************************************************************/
void ficlCompilePlatform(FICL_SYSTEM *pSys)
{
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    dictAppendWord(dp, "heap?",     freeHeap,       FW_DEFAULT);
    dictAppendWord(dp, "dictthreshold", ficlDictThreshold, FW_DEFAULT);
    dictAppendWord(dp, "dictincrease", ficlDictIncrease, FW_DEFAULT);

    dictAppendWord(dp, "ccall",	    ficlCcall,	    FW_DEFAULT);
    return;
}

