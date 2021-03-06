/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief Nanokernel sleep routines
 *
 * This module provides various nanokernel related sleep routines.
 */

#include <nano_private.h>
#include <nano_internal.h>
#include <toolchain.h>
#include <sections.h>
#include <wait_q.h>

void fiber_sleep(int32_t timeout_in_ticks)
{
	int key;

	if (timeout_in_ticks == TICKS_NONE) {
		fiber_yield();
		return;
	}

	key = irq_lock();
	_nano_timeout_add(_nanokernel.current, NULL, timeout_in_ticks);
	_Swap(key);
}

FUNC_ALIAS(_fiber_wakeup, isr_fiber_wakeup, void);
FUNC_ALIAS(_fiber_wakeup, fiber_fiber_wakeup, void);

void _fiber_wakeup(nano_thread_id_t fiber)
{
	int key = irq_lock();

	/* verify first if fiber is not waiting on an object */
	if (!fiber->nano_timeout.wait_q && (_nano_timeout_abort(fiber) == 0)) {
		_nano_fiber_ready(fiber);
	}

	irq_unlock(key);
}

void task_fiber_wakeup(nano_thread_id_t fiber)
{
	int key = irq_lock();

	/* verify first if fiber is not waiting on an object */
	if ((fiber->nano_timeout.wait_q) || (_nano_timeout_abort(fiber) < 0)) {
		irq_unlock(key);
	} else {
		_nano_fiber_ready(fiber);
		_Swap(key);
	}
}

void fiber_wakeup(nano_thread_id_t fiber)
{
	static void (*func[3])(nano_thread_id_t) = {
		isr_fiber_wakeup,
		fiber_fiber_wakeup,
		task_fiber_wakeup
	};

	func[sys_execution_context_type_get()](fiber);
}

#ifndef CONFIG_MICROKERNEL
FUNC_ALIAS(_nano_task_sleep, task_sleep, void);
#endif

void _nano_task_sleep(int32_t timeout_in_ticks)
{
	int64_t  cur_ticks, limit;
	int  key;

	key = irq_lock();
	cur_ticks = sys_tick_get();
	limit = cur_ticks + timeout_in_ticks;

	while (cur_ticks < limit) {
		_NANO_TIMEOUT_SET_TASK_TIMEOUT(timeout_in_ticks);
		nano_cpu_atomic_idle(key);

		key = irq_lock();
		cur_ticks = sys_tick_get();
	}

	irq_unlock(key);
}
