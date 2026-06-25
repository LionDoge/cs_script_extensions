#pragma once
#include "common.h"
#include <cstdint>
#include <vprof.h>
// Hacky workaround for MSVC C2398 narrowing conversion with __LINE__

#ifdef VPROF_ENABLED
#undef VPROF_
#undef VPROF_0
#undef VPROF_BUDGET_FLAGS
#undef VPROF_BUDGET

#define	VPROF_( name, detail, group, bAssertAccounted, budgetFlags )		VPROF_##detail(name,group, bAssertAccounted, budgetFlags)
#define VPROF_BUDGET_FLAGS( name, group, flags )	VPROF_(name, 0, group, false, flags)
#define VPROF_BUDGET( name, group )					VPROF_BUDGET_FLAGS(name, group, BUDGETFLAG_OTHER)

#define	VPROF_0(name,group,assertAccounted,budgetFlags)	VProfScopeHelper<0, assertAccounted> vprofHelper_(name, group, budgetFlags, { __FILE__, static_cast<uint64>(__LINE__), __func__ });

#if VPROF_LEVEL > 0 
#undef VPROF_1
#define	VPROF_1(name,group,assertAccounted,budgetFlags)	VProfScopeHelper<1, assertAccounted> vprofHelper_(name, group, budgetFlags, { __FILE__, static_cast<uint64>(__LINE__), __func__ });
#endif

#if VPROF_LEVEL > 1 
#undef VPROF_2
#define	VPROF_2(name,group,assertAccounted,budgetFlags)	VProfScopeHelper<2, assertAccounted> vprofHelper_(name, group, budgetFlags, { __FILE__, static_cast<uint64>(__LINE__), __func__ });
#endif

#if VPROF_LEVEL > 2 
#undef VPROF_3
#define	VPROF_3(name,group,assertAccounted,budgetFlags)	VProfScopeHelper<3, assertAccounted> vprofHelper_(name, group, budgetFlags, { __FILE__, static_cast<uint64>(__LINE__), __func__ });
#endif

#if VPROF_LEVEL > 3
#undef VPROF_4
#define	VPROF_4(name,group,assertAccounted,budgetFlags)	VProfScopeHelper<4, assertAccounted> vprofHelper_(name, group, budgetFlags, { __FILE__, static_cast<uint64>(__LINE__), __func__ });
#endif

#endif