/**
 * =========================================================================
 * File        : module_init.h
 * Project     : 0 A.D.
 * Description : helpers for module initialization/shutdown.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_MODULE_INIT
#define INCLUDED_MODULE_INIT

/**
 * initialization state of a module: class, source file, or whatever.
 *
 * can be declared as a static variable => no initializer needed,
 * since 0 is the correct initial value.
 *
 * DO NOT change the value directly! (that'd break the carefully thought-out
 * lock-free implementation)
 **/
typedef uintptr_t ModuleInitState;	// uintptr_t required by cpu_CAS

/**
 * @return whether initialization should go forward, i.e. initState is
 * currently MODULE_UNINITIALIZED. increments initState afterwards.
 *
 * (the reason for this function - and tricky part - is thread-safety)
 **/
extern bool ModuleShouldInitialize(volatile ModuleInitState* initState);

/**
 * if module reference count is valid, decrement it.
 * @return whether shutdown should go forward, i.e. this is the last
 * shutdown call.
 **/
extern bool ModuleShouldShutdown(volatile ModuleInitState* initState);

/**
 * indicate the module is unusable, e.g. due to failure during init.
 * all subsequent ModuleShouldInitialize/ModuleShouldShutdown calls
 * for this initState will return false.
 **/
extern void ModuleSetError(volatile ModuleInitState* initState);

#endif	// #ifndef INCLUDED_MODULE_INIT
