#pragma once

static uintptr_t __cdecl I_beginthreadex(
	void*					 _Security,
	unsigned                 _StackSize,
	_beginthreadex_proc_type _StartAddress,
	void*					 _ArgList,
	unsigned                 _InitFlag,
	unsigned*				 _ThrdAddr) {

	return iat(_beginthreadex).get()(_Security, _StackSize, _StartAddress, _ArgList, _InitFlag, _ThrdAddr);
}