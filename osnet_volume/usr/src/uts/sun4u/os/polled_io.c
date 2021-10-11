/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)polled_io.c	1.3	99/09/15 SMI"

/*
 * This code sets up the callbacks(vx_handlers) so that the firmware may call
 * into the kernel for console input and/or output while in the debugger.
 * The callbacks that execute in debug mode must be careful to not
 * allocate memory, access mutexes, etc. because most kernel services are
 * not available during this mode.
 *
 * This code, and the underlying code that supports the polled input, is very
 * hard to debug.  In order to get the code to execute, polled input must
 * provide input to the debugger.  If anything goes wrong with the code, then
 * it is hard to debug the debugger.  If there are any problems to debug,
 * the following is useful:
 *
 * set the polled_debug variable in /etc/system
 *	set polled_debug=1
 *
 * This variable will register the callbacks but will not throw the switch
 * in the firmware.  The callbacks can be executed by hand from the firmware.
 * Boot the system and drop down to the firmware.
 *
 *	ok " /os-io" select-dev
 *
 * The following will cause the polled_give_input to execute:
 *	ok take
 *
 * The following will cause the polled_take_input to execute:
 *	ok give
 *
 * The following will cause polled_read to execute:
 *	ok read
 */

#include <sys/stropts.h>
#include <v9/sys/prom_isa.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/note.h>
#include <sys/consdev.h>
#include <sys/polled_io.h>
#include <sys/debug/debug.h>

/*
 * Internal Functions
 */
static void	polled_give_input(cell_t *cif);
static void	polled_read(cell_t *cif);
static void	polled_take_input(cell_t *cif);
static void	polled_give_output(cell_t *cif);
static void	polled_write(cell_t *cif);
static void	polled_take_output(cell_t *cif);
static void	polled_io_register(cons_polledio_t *,
			polled_io_console_type_t, int);
static void	polled_io_unregister(polled_io_console_type_t, int);
static int	polled_io_take_console(polled_io_console_type_t, int);
static int	polled_io_release_console(polled_io_console_type_t, int);

/*
 * State information regarding the input/output device
 */
static polled_device_t	polled_input_device;
static polled_device_t	polled_output_device;
static int polled_vx_handlers_init = 0;

extern void	add_vx_handler(char *name, int flag, void (*func)(cell_t *));
extern void	remove_vx_handler(char *name);

/*
 * This structure is used to communicate with kadb.  If the system
 * is booted with kadb, then this structure will be non-NULL
 */
extern struct debugvec *dvec;

/*
 * This is a useful flag for debugging the entry points.   This flag
 * allows us to exercise the entry points from the firmware without
 * switching the firmware's notion of the input device.
 */
int	polled_debug = 0;

/*
 * This routine is called to initialize polled I/O.  We insert our entry
 * points so that the firmware will call into this code
 * when the switch is thrown in polled_io_take_console().
 */
void
polled_io_init(void)
{

	/*
	 * Only do the initialization once
	 */
	if (polled_vx_handlers_init != 0)
		return;

	/*
	 * Add the vx_handlers for the different functions that
	 * need to be accessed from firmware.
	 */
	add_vx_handler("enter-input", 1, polled_give_input);

	add_vx_handler("read", 1, polled_read);

	add_vx_handler("exit-input", 1, polled_take_input);

	add_vx_handler("give-output", 1, polled_give_output);

	add_vx_handler("write", 1, polled_write);

	add_vx_handler("take-output", 1, polled_take_output);

	/*
	 * Initialize lock to protect multiple thread access to the
	 * polled_input_device structure.  This does not protect
	 * us from access in debug mode.
	 */
	mutex_init(&polled_input_device.polled_device_lock,
		NULL, MUTEX_DRIVER, NULL);

	/*
	 * Initialize lock to protect multiple thread access to the
	 * polled_output_device structure.  This does not protect
	 * us from access in debug mode.
	 */
	mutex_init(&polled_output_device.polled_device_lock,
		NULL, MUTEX_DRIVER, NULL);

	polled_vx_handlers_init = 1;
}

/*
 * Register a device for input or output.  The polled_io structure
 * will be filled in with the callbacks that are appropriate for
 * that device.
 */
int
polled_io_register_callbacks(
cons_polledio_t			*polled_io,
int				flags
)
{
	/*
	 * If the input structure entries aren't filled in, then register this
	 * structure as an input device.
	 */
	if ((polled_io->cons_polledio_getchar != NULL) &&
		(polled_io->cons_polledio_ischar != NULL)) {

		polled_io_register(polled_io,
			POLLED_IO_CONSOLE_INPUT, flags);
	}

	/*
	 * If the output structure entries aren't filled in, then register this
	 * structure as an output device.
	 */
	if (polled_io->cons_polledio_putchar != NULL) {

		polled_io_register(polled_io,
			POLLED_IO_CONSOLE_OUTPUT, flags);
	}

	/*
	 * If the dvec is non-NULL, then the system booted
	 * with kadb.
	 */
	if (dvec != NULL) {

		/*
		 * Send the polled_io structure to kadb
		 */
		(*dvec->dv_set_polled_callbacks)(polled_io);
	}

	return (DDI_SUCCESS);
}

/*
 * Unregister a device for console input/output.
 */
int
polled_io_unregister_callbacks(
cons_polledio_t			*polled_io,
int				flags
)
{
	/*
	 * If polled_io is being used for input, then unregister it.
	 */
	if (polled_io == polled_input_device.polled_io) {

		polled_io_unregister(
			POLLED_IO_CONSOLE_INPUT, flags);
	}

	/*
	 * If polled_io is being used for output, then unregister it.
	 */
	if (polled_io == polled_output_device.polled_io) {

		polled_io_unregister(
			POLLED_IO_CONSOLE_OUTPUT, flags);
	}

	return (DDI_SUCCESS);
}

/*
 * This routine is called when we are done handling polled io.  We will
 * remove all of our handlers and destroy any memory that we have allocated.
 */
void
polled_io_fini()
{
	/*
	 * Remove the vx_handlers so that our functions will nolonger be
	 * accessible.
	 */
	remove_vx_handler("enter-input");

	remove_vx_handler("read");

	remove_vx_handler("exit-input");

	remove_vx_handler("give-output");

	remove_vx_handler("write");

	remove_vx_handler("take-output");

	/*
	 * Destroy the mutexes, we will not need them anymore.
	 */
	mutex_destroy(&polled_input_device.polled_device_lock);

	mutex_destroy(&polled_output_device.polled_device_lock);

	polled_vx_handlers_init = 0;
}

/*
 * Generic internal routine for registering a polled input or output device.
 */
/* ARGSUSED */
static void
polled_io_register(
cons_polledio_t			*polled_io,
polled_io_console_type_t	type,
int				flags
)
{
	switch (type) {
	case POLLED_IO_CONSOLE_INPUT:
		/*
		 * Grab the device lock, because we are going to access
		 * protected structure entries.  We do this before the
		 * POLLED_IO_CONSOLE_OPEN_INPUT so that we serialize
		 * registration.
		 */
		mutex_enter(&polled_input_device.polled_device_lock);

		/*
		 * Save the polled_io pointers so that we can access
		 * them later.
		 */
		polled_input_device.polled_io = polled_io;

		mutex_exit(&polled_input_device.polled_device_lock);


		if (!polled_debug) {
			/*
			 * Tell the generic console framework to
			 * repoint firmware's stdin to this keyboard device.
			 */
			(void) polled_io_take_console(type, 0);
		}

		break;

	case POLLED_IO_CONSOLE_OUTPUT:
		/*
		 * Grab the device lock, because we are going to access
		 * protected structure entries. We do this before the
		 * POLLED_IO_CONSOLE_OPEN_OUTPUT so that we serialize
		 * registration.
		 */
		mutex_enter(&polled_output_device.polled_device_lock);

		/*
		 * Save the polled_io pointers so that we can access
		 * them later.
		 */
		polled_input_device.polled_io = polled_io;

		mutex_exit(&polled_output_device.polled_device_lock);

		break;
	}
}

/*
 * Generic internal routine for unregistering a polled input or output device.
 */
/* ARGSUSED */
static void
polled_io_unregister(
polled_io_console_type_t	type,
int				flags
)
{
	switch (type) {
	case POLLED_IO_CONSOLE_INPUT:
		/*
		 * Tell the generic console framework to restore
		 * the firmware's old stdin pointers.
		 */
		(void) polled_io_release_console(type, 0);

		/*
		 * Grab the device lock, because we are going to access
		 * protected structure entries.
		 */
		mutex_enter(&polled_input_device.polled_device_lock);

		polled_input_device.polled_io = NULL;

		mutex_exit(&polled_input_device.polled_device_lock);

		break;

	case POLLED_IO_CONSOLE_OUTPUT:
		/*
		 * Grab the device lock, because we are going to access
		 * protected structure entries.
		 */
		mutex_enter(&polled_output_device.polled_device_lock);

		polled_output_device.polled_io = NULL;

		mutex_exit(&polled_output_device.polled_device_lock);

		break;
	}
}

/*
 * This is the routine that is called to throw the switch from the
 * firmware's ownership of stdout/stdin to the kernel.
 */
/* ARGSUSED */
static int
polled_io_take_console(
polled_io_console_type_t	type,
int				flags
)
{

	switch (type) {
	case POLLED_IO_CONSOLE_INPUT:
		/*
		 * Call into firmware to switch to the kernel I/O handling.
		 * We will save the old value of stdin so that we can
		 * restore it if the device is released.
		 */
#ifdef DEBUG_OBP
		/*
		 * This code is useful to trace through
		 * what the prom is doing
		 */
		prom_interpret(
			"stdin @ swap ! trace-on \" /os-io\" input trace-off",
			(uintptr_t)&polled_input_device.polled_old_handle,
				0, 0, 0, 0);
#endif

		prom_interpret(
			"stdin @ swap ! \" /os-io\" open-dev stdin !",
			(uintptr_t)&polled_input_device.polled_old_handle,
				0, 0, 0, 0);

		break;

	case POLLED_IO_CONSOLE_OUTPUT:
		/*
		 * Call into firmware to switch to the kernel I/O handling.
		 * We will save the old value of stdout so that we can
		 * restore it if the device is released.
		 */
		prom_interpret(
			"stdout @ swap ! \" /os-io\" output",
			(uintptr_t)&polled_output_device.polled_old_handle,
				0, 0, 0, 0);

		break;
	}

	return (DDI_SUCCESS);
}

/*
 * This routine gives control of console input/output back to firmware.
 */
/* ARGSUSED */
static int
polled_io_release_console(
polled_io_console_type_t	type,
int				flags
)
{
	switch (type) {
	case POLLED_IO_CONSOLE_INPUT:
		/*
		 * Restore the stdin handle
		 */
		prom_interpret("to stdin",
			(uintptr_t)polled_input_device.
				polled_old_handle,
				0, 0, 0, 0);

		break;

	case POLLED_IO_CONSOLE_OUTPUT:
		/*
		 * Restore the stdout handle
		 */
		prom_interpret("to stdout",
			(uintptr_t)polled_output_device.
				polled_old_handle,
				0, 0, 0, 0);

		break;
	}

	return (DDI_SUCCESS);
}


/*
 * This is the routine that the firmware calls to save any state information
 * before using the input device.  This routine, and all of the
 * routines that it calls, are responsible for saving any state
 * information so that it can be restored when debug mode is over.
 *
 * WARNING: This routine runs in debug mode.
 */
static void
polled_give_input(cell_t *cif)
{
	cons_polledio_t		*polled_io;
	uint_t			out_args;

	/*
	 * Calculate the offset of the return arguments
	 */
	out_args = CIF_MIN_SIZE +
		p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * There is one argument being passed back to firmware.
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);
	cif[out_args] = p1275_uint2cell(CIF_SUCCESS);

	/*
	 * We check to see if there is an
	 * input device that has been registered.
	 */
	polled_io = polled_input_device.polled_io;

	if (polled_io == NULL) {
		return;
	}

	/*
	 * Call down to the lower layers to save the state.
	 */
	polled_io->cons_polledio_enter(
		polled_io->cons_polledio_argument);
}

/*
 * This is the routine that the firmware calls
 * when it wants to read a character.
 * We will call to the lower layers to see if there is any input data
 * available.
 *
 * WARNING: This routine runs in debug mode.
 */
static void
polled_read(cell_t *cif)
{
	uint_t				actual;
	cons_polledio_t			*polled_io;
	uint_t				in_args;
	uint_t				out_args;
	uchar_t				*buffer;
	uint_t				buflen;
	uchar_t				key;

	/*
	 * The number of arguments passed in by the firmware
	 */
	in_args = p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * Calculate the location of the first out arg.  This location is
	 * CIF_MIN_SIZE plus the in argument locations.
	 */
	out_args = CIF_MIN_SIZE + in_args;

	/*
	 * The firmware should pass in a pointer to a buffer, and the
	 * number of characters it expects or expects to write.
	 * If 2 arguments are not passed in, then return an error.
	 */
	if (in_args != 2) {

		/*
		 * Tell firmware how many arguments we are passing back.
		 */
		cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);

		/*
		 * Tell the firmware that we cannot give it any characters.
		 */
		cif[out_args] = p1275_uint2cell(CIF_FAILURE);

		return;
	}

	/*
	 * Get the address of where to copy the characters into.
	 */
	buffer = (uchar_t *)p1275_cell2uint(cif[CIF_MIN_SIZE+0]);

	/*
	 * Get the length of the buffer that we can copy characters into.
	 */
	buflen = p1275_cell2uint(cif[CIF_MIN_SIZE+1]);

	/*
	 * Make sure there is enough room in the buffer to copy the
	 * characters into.
	 */
	if (buflen == 0) {

		/*
		 * Tell the OBP that we cannot give it any characters.
		 */
		cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);

		/*
		 * Tell the firmware that we cannot give it any characters.
		 */
		cif[out_args] = p1275_uint2cell(CIF_FAILURE);

		return;
	}

	/*
	 * Pass back whether or not the operation was a success or
	 * failure plus the actual number of bytes in the buffer.
	 * Tell firmware how many arguments we are passing back.
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)2);

	/*
	 * Initialize the cif to be "no characters"
	 */
	cif[out_args+0] = p1275_uint2cell(CIF_SUCCESS);
	cif[out_args+1] = p1275_uint2cell(CIF_NO_CHARACTERS);

	/*
	 * We check to see if there is an
	 * input device that has been registered.
	 */
	polled_io = polled_input_device.polled_io;

	if (polled_io == NULL) {

		/*
		 * The cif structure is already set up to return
		 * no characters.
		 */

		return;
	}

	actual = 0;

	/*
	 * Obtain the characters
	 */
	while (polled_io->cons_polledio_ischar(
		polled_io->cons_polledio_argument) == B_TRUE) {

		/*
		 * Make sure that we don't overrun the buffer.
		 */
		if (actual == buflen) {

			break;
		}

		/*
		 * Call down to the device to copy the input data into the
		 * buffer.
		 */
		key = polled_io->cons_polledio_getchar(
			polled_io->cons_polledio_argument);

		*(buffer + actual) = key;

		actual++;
	}

	/*
	 * There is a special return code when there is no data.
	 */
	if (actual == 0) {

		/*
		 * The cif structure is already set up to return
		 * no characters.
		 */

		return;
	}

	/*
	 * Tell firmware how many characters we are sending it.
	 */
	cif[out_args+0] = p1275_uint2cell((uint_t)CIF_SUCCESS);
	cif[out_args+1] = p1275_uint2cell((uint_t)actual);
}

/*
 * This is the routine that firmware calls when it is giving up control of the
 * input device.  This routine, and the lower layer routines that it calls,
 * are responsible for restoring the controller state to the state it was
 * in before firmware took control.
 *
 * WARNING: This routine runs in debug mode.
 */
static void
polled_take_input(cell_t *cif)
{
	cons_polledio_t		*polled_io;
	uint_t			out_args;

	/*
	 * Calculate the offset of the return arguments
	 */
	out_args = CIF_MIN_SIZE +
		p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * There is one argument being passed back to firmware.
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);
	cif[out_args] = p1275_uint2cell(CIF_SUCCESS);

	/*
	 * We check the pointer to see if there is an
	 * input device that has been registered.
	 */
	polled_io = polled_input_device.polled_io;

	if (polled_io == NULL) {
		return;
	}

	/*
	 * Call down to the lower layers to save the state.
	 */
	polled_io->cons_polledio_exit(
		polled_io->cons_polledio_argument);
}

/*
 * This is the routine that the firmware calls to save any state information
 * before using the output device.  This routine, and all of the
 * routines that it calls, are responsible for saving any state
 * information so that it can be restored when the debug  is over.
 *
 * WARNING:  This routine runs in debug mode.
 */
static void
polled_give_output(cell_t *cif)
{
	cons_polledio_t		*polled_io;

	uint_t			out_args;

	/*
	 * Calculate the offset of the return arguments
	 */
	out_args = CIF_MIN_SIZE +
		p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * There is one argument being passed back to the firmware .
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);
	cif[out_args] = p1275_uint2cell(CIF_SUCCESS);

	/*
	 * We check to see if there is an
	 * output device that has been registered.
	 */
	polled_io = polled_output_device.polled_io;

	if (polled_io == NULL) {
		return;
	}

	/*
	 * Call down to the lower layers to save the state.
	 */
	polled_io->cons_polledio_enter(
		polled_io->cons_polledio_argument);
}

/*
 * This is the routine that the firmware calls when
 * it wants to write a character.
 *
 * WARNING: This routine runs in debug mode.
 */
static void
polled_write(cell_t *cif)
{
	cons_polledio_t			*polled_io;
	uint_t				in_args;
	uint_t				out_args;
	uchar_t				*buffer;
	uint_t				buflen;
	uint_t				i;

	/*
	 * The number of arguments passed in by the firmware
	 */
	in_args = p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * Calculate the location of the first out arg.  This location is
	 * CIF_MIN_SIZE (name + no. in args + no. out args) plus the
	 * in argument locations.
	 */
	out_args = CIF_MIN_SIZE + in_args;

	/*
	 * The firmware should pass in a pointer to a buffer, and the
	 * number of characters it expects or expects to write.
	 * If 2 arguments are not passed in, then return an error.
	 */
	if (in_args != 2) {

		/*
		 * Tell firmware how many arguments we are passing back.
		 */
		cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);


		/*
		 * Tell the firmware that we cannot give it any characters.
		 */
		cif[out_args] = p1275_uint2cell(CIF_FAILURE);

		return;
	}

	/*
	 * Get the address of where to copy the characters into.
	 */
	buffer = (uchar_t *)p1275_cell2uint(cif[CIF_MIN_SIZE+0]);

	/*
	 * Get the length of the buffer that we can copy characters into.
	 */
	buflen = p1275_cell2uint(cif[CIF_MIN_SIZE+1]);

	/*
	 * Make sure there is enough room in the buffer to copy the
	 * characters into.
	 */
	if (buflen == 0) {

		/*
		 * Tell the OBP that we cannot give it any characters.
		 */
		cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);

		/*
		 * Tell the firmware that we cannot give it any characters.
		 */
		cif[out_args] = p1275_uint2cell(CIF_FAILURE);

		return;
	}


	/*
	 * Tell the firmware how many arguments we are passing back.
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)2);

	/*
	 * Initialize the cif to success
	 */
	cif[out_args+0] = p1275_uint2cell(CIF_SUCCESS);
	cif[out_args+1] = p1275_uint2cell(0);

	/*
	 * We check the pointer to see if there is an
	 * input device that has been registered.
	 */
	polled_io = polled_output_device.polled_io;

	if (polled_io == NULL) {

		/*
		 * The cif is already initialized
		 */
		return;
	}

	for (i = 0; i < buflen; i++) {

		polled_io->cons_polledio_putchar(
			polled_io->cons_polledio_argument, *(buffer + i));
	}

	/*
	 * Tell the firmware how many characters we are sending it.
	 */
	cif[out_args+0] = p1275_uint2cell((uint_t)CIF_SUCCESS);
	cif[out_args+1] = p1275_uint2cell((uint_t)buflen);
}

/*
 * This is the routine that the firmware calls
 *  when it is giving up control of the
 * output device.  This routine, and the lower layer routines that it calls,
 * are responsible for restoring the controller state to the state it was
 * in before the firmware took control.
 *
 * WARNING: This routine runs in debug mode.
 */
static void
polled_take_output(cell_t *cif)
{
	cons_polledio_t		*polled_io;
	uint_t			out_args;

	/*
	 * Calculate the offset of the return arguments
	 */
	out_args = CIF_MIN_SIZE +
		p1275_cell2uint(cif[CIF_NUMBER_IN_ARGS]);

	/*
	 * There is one argument being passed back to the firmware.
	 */
	cif[CIF_NUMBER_OUT_ARGS] = p1275_uint2cell((uint_t)1);
	cif[out_args] = p1275_uint2cell(CIF_SUCCESS);

	/*
	 * We check the pointer to see if there is an
	 * output device that has been registered.
	 */
	polled_io = polled_output_device.polled_io;

	if (polled_io == NULL) {
		return;
	}

	/*
	 * Call down to the lower layers to save the state.
	 */
	polled_io->cons_polledio_exit(
		polled_io->cons_polledio_argument);
}
