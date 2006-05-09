/**************************************************************************

 Copyright 2006 Dave Airlie <airlied@linux.ie>
 
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "i830.h"
#include "i830_sdvo_regs.h"

unsigned short curr_table[6];
unsigned short out_timings[6];


/* SDVO support for i9xx chipsets */
static Bool sReadByte(I830SDVOPtr s, int addr, unsigned char *ch)
{
  if (!xf86I2CReadByte(&(s->d), addr, ch)) {
    xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR, "Unable to read from %s Slave %d.\n", s->d.pI2CBus->BusName, s->d.SlaveAddr);
    return FALSE;
  }
  return TRUE;
}

static Bool sWriteByte(I830SDVOPtr s, int addr, unsigned char ch)
{
  if (!xf86I2CWriteByte(&(s->d), addr, ch)) {
    xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR, "Unable to write to %s Slave %d.\n", s->d.pI2CBus->BusName, s->d.SlaveAddr);
    return FALSE;
  }
  return TRUE;
}

/* following on from tracing the intel BIOS i2c routines */
Bool
I830SDVOWriteOutputs(I830SDVOPtr s, int num_out)
{
  int i;

  ErrorF("SDVO: W: ");
  for (i = num_out; i<9; i++)
    ErrorF("%02X ", s->sdvo_regs[i]);
  ErrorF("\n");
  /* blast the output regs */
  for (i = 7; i >= num_out; i--)
  {
    sWriteByte(s, i, s->sdvo_regs[i]);
  }
  /* blast the command reg */
  sWriteByte(s, 8, s->sdvo_regs[8]);
}

static const char *cmd_status_names[] = {
	"Power on",
	"Success",
	"Not supported",
	"Invalid arg",
	"Pending",
	"Target not supported",
	"Scaling not supported"
};


static void
I830SDVOReadInputRegs(I830SDVOPtr s)
{
  int i;
  
  sReadByte(s, SDVO_I2C_CMD_STATUS, &s->sdvo_regs[SDVO_I2C_CMD_STATUS]);
  
  sReadByte(s, SDVO_I2C_RETURN_3, &s->sdvo_regs[SDVO_I2C_RETURN_3]);
  sReadByte(s, SDVO_I2C_RETURN_2, &s->sdvo_regs[SDVO_I2C_RETURN_2]);
  sReadByte(s, SDVO_I2C_RETURN_1, &s->sdvo_regs[SDVO_I2C_RETURN_1]);
  sReadByte(s, SDVO_I2C_RETURN_0, &s->sdvo_regs[SDVO_I2C_RETURN_0]);
  sReadByte(s, SDVO_I2C_RETURN_7, &s->sdvo_regs[SDVO_I2C_RETURN_7]);
  sReadByte(s, SDVO_I2C_RETURN_6, &s->sdvo_regs[SDVO_I2C_RETURN_6]);
  sReadByte(s, SDVO_I2C_RETURN_5, &s->sdvo_regs[SDVO_I2C_RETURN_5]);
  sReadByte(s, SDVO_I2C_RETURN_4, &s->sdvo_regs[SDVO_I2C_RETURN_4]);

  ErrorF("SDVO: R: ");
  for (i = SDVO_I2C_RETURN_0; i <= SDVO_I2C_RETURN_7; i++)
    ErrorF("%02X ", s->sdvo_regs[i]);
  if (s->sdvo_regs[SDVO_I2C_CMD_STATUS] <= SDVO_CMD_STATUS_SCALING_NOT_SUPP)
    ErrorF("(%s)", cmd_status_names[s->sdvo_regs[SDVO_I2C_CMD_STATUS]]);
  else
    ErrorF("(??? %d)", s->sdvo_regs[SDVO_I2C_CMD_STATUS]);
  ErrorF("\n");
}

/* Sets the control bus switch to either point at one of the DDC buses or the
 * PROM.  It resets from the DDC bus back to internal registers at the next I2C
 * STOP.  PROM access is terminated by accessing an internal register.
 */
Bool
I830SDVOSetControlBusSwitch(I830SDVOPtr s, CARD8 target)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CONTROL_BUS_SWITCH;
    s->sdvo_regs[SDVO_I2C_ARG_0] = target;

    I830SDVOWriteOutputs(s, 1);
    return TRUE;
}


static Bool
I830SDVOSetTargetInput(I830SDVOPtr s, Bool target_1, Bool target_2)
{
  /* write out 0x10 */
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_INPUT;
  //  s->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
  //  s->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

  I830SDVOWriteOutputs(s, 0);
  
  I830SDVOReadInputRegs(s);

  return TRUE;
}

static Bool
I830SDVOGetTrainedInputs(I830SDVOPtr s)
{
  memset(s->sdvo_regs, 0, 9);

  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_TRAINED_INPUTS;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

static Bool
I830SDVOGetActiveOutputs(I830SDVOPtr s, Bool *on_1, Bool *on_2)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ACTIVE_OUTPUTS;

  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);

  *on_1 = s->sdvo_regs[SDVO_I2C_ARG_0];
  *on_2 = s->sdvo_regs[SDVO_I2C_ARG_1];
  
  return TRUE;
}


static Bool
I830SDVOSetActiveOutputs(I830SDVOPtr s, Bool on_1, Bool on_2)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_ACTIVE_OUTPUTS;

  s->sdvo_regs[SDVO_I2C_ARG_0] = on_1;
  s->sdvo_regs[SDVO_I2C_ARG_1] = on_2;

  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}


static Bool
I830SDVOGetInputPixelClockRange(I830SDVOPtr s, CARD16 *clock_min, CARD16 *clock_max)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);

  *clock_min = s->sdvo_regs[SDVO_I2C_RETURN_0] |
    (s->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
  *clock_max = s->sdvo_regs[SDVO_I2C_RETURN_2] |
    (s->sdvo_regs[SDVO_I2C_RETURN_3] << 8);  

  return TRUE;
}


static Bool
I830SDVOSetTargetOutput(I830SDVOPtr s, Bool target_1, Bool target_2)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_OUTPUT;
  
  s->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
  s->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}


static Bool
I830SDVOGetOutputTimingsPart1(I830SDVOPtr s)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_OUTPUT_TIMINGS_PART1;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

static Bool
I830SDVOGetOutputTimingsPart2(I830SDVOPtr s)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_OUTPUT_TIMINGS_PART2;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}



static Bool
I830SDVOSetTimingsPart1(I830SDVOPtr s, char cmd, unsigned short clock, CARD16 magic1, CARD16 magic2, CARD16 magic3)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
  
  /* set clock regs */
  s->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_2] = magic3 & 0xff;  
  s->sdvo_regs[SDVO_I2C_ARG_3] = (magic3 >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_4] = magic2 & 0xff;  
  s->sdvo_regs[SDVO_I2C_ARG_5] = (magic2 >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_6] = magic1 & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_7] = (magic1 >> 8) & 0xff;


  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

static Bool
I830SDVOSetInputTimingsPart1(I830SDVOPtr s, unsigned short clock, unsigned short magic1, unsigned short magic2, unsigned short magic3)
{
  return I830SDVOSetTimingsPart1(s, SDVO_CMD_SET_INPUT_TIMINGS_PART1, clock, magic1, magic2, magic3);
}

static Bool
I830SDVOSetOutputTimingsPart1(I830SDVOPtr s, unsigned short clock, unsigned short magic1, unsigned short magic2, unsigned short magic3)
{
  return I830SDVOSetTimingsPart1(s, SDVO_CMD_SET_OUTPUT_TIMINGS_PART1, clock, magic1, magic2, magic3);
}

static Bool
I830SDVOSetTimingsPart2(I830SDVOPtr s, char cmd, unsigned short magic4, unsigned short magic5, unsigned short magic6)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
 
  /* set clock regs */
  s->sdvo_regs[SDVO_I2C_ARG_0] = magic4 & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_1] = (magic4 >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_2] = magic5 & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_3] = (magic5 >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_4] = magic6 & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_5] = (magic6 >> 8) & 0xff;

  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

static Bool
I830SDVOSetInputTimingsPart2(I830SDVOPtr s, unsigned short magic4, unsigned short magic5, unsigned short magic6)
{
  return I830SDVOSetTimingsPart2(s, SDVO_CMD_SET_INPUT_TIMINGS_PART2, magic4, magic5, magic6);
}

static Bool
I830SDVOSetOutputTimingsPart2(I830SDVOPtr s, unsigned short magic4, unsigned short magic5, unsigned short magic6)
{
  return I830SDVOSetTimingsPart2(s, SDVO_CMD_SET_OUTPUT_TIMINGS_PART2, magic4, magic5, magic6);
}

static Bool
I830SDVOCreatePreferredInputTiming(I830SDVOPtr s, unsigned short clock, unsigned short width, unsigned short height)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING;
  
  /* set clock regs */
  s->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_2] = width & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_3] = (width >> 8) & 0xff;

  s->sdvo_regs[SDVO_I2C_ARG_4] = height & 0xff;
  s->sdvo_regs[SDVO_I2C_ARG_5] = (height >> 8) & 0xff;

  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

static Bool
I830SDVOGetPreferredInputTimingPart1(I830SDVOPtr s)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);

  curr_table[0] = (s->sdvo_regs[0x10] | (s->sdvo_regs[0x11] << 8));
  curr_table[1] = (s->sdvo_regs[0x0e] | (s->sdvo_regs[0x0f] << 8));
  curr_table[2] = (s->sdvo_regs[0x0c] | (s->sdvo_regs[0x0d] << 8));  
  
  return TRUE;
}


static Bool
I830SDVOGetPreferredInputTimingPart2(I830SDVOPtr s)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2;
  
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);

  curr_table[3] = (s->sdvo_regs[0x0a] | (s->sdvo_regs[0x0b] << 8));
  curr_table[4] = (s->sdvo_regs[0x0c] | (s->sdvo_regs[0x0d] << 8));
  curr_table[5] = 0x1e;

  return TRUE;  
}

static Bool
I830SDVOSetClockRateMult(I830SDVOPtr s, unsigned char val)
{
  memset(s->sdvo_regs, 0, 9);
  
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CLOCK_RATE_MULT;
  
  s->sdvo_regs[SDVO_I2C_ARG_0] = val;
  I830SDVOWriteOutputs(s, 1);
  I830SDVOReadInputRegs(s);
  
  return TRUE;
}

Bool
I830SDVOPreSetMode(I830SDVOPtr s, DisplayModePtr mode)
{
  unsigned short clock = mode->Clock/10, width=mode->CrtcHDisplay, height=mode->CrtcVDisplay;
  unsigned short *m_p;
  unsigned short h_blank_len, h_sync_len, v_blank_len, v_sync_len, h_sync_offset, v_sync_offset;
  unsigned short sync_flags;
  Bool out1, out2;
  CARD16 min_clock, max_clock;

  /* do some mode translations */
  h_blank_len = mode->CrtcHBlankEnd - mode->CrtcHBlankStart;
  h_sync_len = mode->CrtcHSyncEnd - mode->CrtcHSyncStart;
  
  v_blank_len = mode->CrtcVBlankEnd - mode->CrtcVBlankStart;
  v_sync_len = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;
  
  h_sync_offset = mode->CrtcHSyncStart - mode->CrtcHBlankStart;
  v_sync_offset = mode->CrtcVSyncStart - mode->CrtcVBlankStart;

  sync_flags = 0x18;
  if (mode->Flags & V_PHSYNC)
    sync_flags |= 0x2;
  if (mode->Flags & V_PVSYNC)
    sync_flags |= 0x4;

  out_timings[0] = (v_blank_len & 0xff);
  out_timings[0] |= ((((height >> 8) & 0xf) << 4) | ((v_blank_len >> 8) & 0xf)) << 8;

  out_timings[1] = ((((width >> 8) & 0xf) << 4) | ((h_blank_len >> 8) & 0xf));
  out_timings[1] |= ((height & 0xff)<<8);

  out_timings[2] = (width & 0xff);
  out_timings[2] |= (h_blank_len & 0xff) << 8;

  out_timings[3] = h_sync_offset | ((h_sync_len & 0xff) << 8);

  out_timings[4] = (v_sync_offset & 0xf) << 4 | (v_sync_len & 0xf);
  out_timings[4] |= 0 << 8;

  out_timings[5] = sync_flags;
  out_timings[5] |= 0 << 8;
  
  I830SDVOSetTargetInput(s, TRUE, TRUE);
  I830SDVOGetInputPixelClockRange(s, &min_clock, &max_clock);

  I830SDVOGetActiveOutputs(s, &out1, &out2);
  I830SDVOSetActiveOutputs(s, FALSE, FALSE);

  I830SDVOSetTargetOutput(s, TRUE, FALSE);
  I830SDVOSetOutputTimingsPart1(s, clock, out_timings[0], out_timings[1], out_timings[2]);
  I830SDVOSetOutputTimingsPart2(s, out_timings[3], out_timings[4],out_timings[5]);

  I830SDVOSetTargetInput(s, TRUE, TRUE);

  I830SDVOCreatePreferredInputTiming(s, clock, width, height);

  I830SDVOGetPreferredInputTimingPart1(s);
  I830SDVOGetPreferredInputTimingPart2(s);

  I830SDVOSetTargetInput(s, TRUE, TRUE);

  I830SDVOSetInputTimingsPart1(s, clock, curr_table[0], curr_table[1], curr_table[2]);
  I830SDVOSetInputTimingsPart2(s, curr_table[3], curr_table[4], out_timings[5]);

  I830SDVOSetTargetInput(s, TRUE, TRUE);
  if (mode->PrivFlags & I830_MFLAG_DOUBLE)
    I830SDVOSetClockRateMult(s, 0x02);
  else
    I830SDVOSetClockRateMult(s, 0x01);
}

Bool
I830SDVOPostSetMode(I830SDVOPtr s, DisplayModePtr mode)
{
  int clock = mode->Clock/10, height=mode->CrtcVDisplay;
  Bool ret = TRUE;
  Bool out1, out2;

  I830SDVOGetTrainedInputs(s);

  /* THIS IS A DIRTY HACK - sometimes for some reason on startup
     the BIOS doesn't find my DVI monitor -
     without this hack the driver doesn't work.. this causes the modesetting
     to be re-run
  */
  if (s->sdvo_regs[0x0a] != 0x1)
  {
    ret = FALSE;
  }

  I830SDVOGetActiveOutputs(s, &out1, &out2);
  I830SDVOSetActiveOutputs(s, TRUE, FALSE);

  I830SDVOSetTargetInput(s, TRUE, TRUE);

  return ret;
}


static Bool
I830SDVODDCI2CGetByte(I2CDevPtr d, I2CByte *data, Bool last)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CGetByte(d, data, last);
    d->pI2CBus = savebus;

    return ret;
}

static Bool
I830SDVODDCI2CPutByte(I2CDevPtr d, I2CByte c)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CPutByte(d, c);
    d->pI2CBus = savebus;

    return ret;
}

static Bool
I830SDVODDCI2CStart(I2CBusPtr b, int timeout)
{
    I830SDVOPtr sdvo = b->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus;

    I830SDVOSetControlBusSwitch(sdvo, SDVO_CONTROL_BUS_DDC2);
    return i2cbus->I2CStart(i2cbus, timeout);
}

static void
I830SDVODDCI2CStop(I2CDevPtr d)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;

    ErrorF("SDVO I2C STOP\n");
    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    i2cbus->I2CStop(d);
    d->pI2CBus = savebus;
}

/* It's a shame that xf86i2c.c's I2CAddress() doesn't use the bus's pointers,
 * so it's useless to us here.
 */
static Bool
I830SDVODDCI2CAddress(I2CDevPtr d, I2CSlaveAddr addr)
{
    if (d->pI2CBus->I2CStart(d->pI2CBus, d->StartTimeout)) {
	if (d->pI2CBus->I2CPutByte(d, addr & 0xFF)) {
	    if ((addr & 0xF8) != 0xF0 &&
		(addr & 0xFE) != 0x00)
		return TRUE;

	    if (d->pI2CBus->I2CPutByte(d, (addr >> 8) & 0xFF))
		return TRUE;
	}

	d->pI2CBus->I2CStop(d);
    }

    return FALSE;
}

I830SDVOPtr
I830SDVOInit(ScrnInfoPtr pScrn, int output_index, CARD32 output_device)
{
  I830Ptr pI830 = I830PTR(pScrn);
  I830SDVOPtr sdvo;
  unsigned char ch[0x40];
  int i;
  I2CBusPtr i2cbus, ddcbus;

  i2cbus = pI830->output[output_index].pI2CBus;

  sdvo = xcalloc(1, sizeof(I830SDVORec));
  if (sdvo == NULL)
    return NULL;

  if (output_device == DVOB) {
    sdvo->d.DevName = "SDVO Controller B";
    sdvo->d.SlaveAddr = 0x70;
  } else {
    sdvo->d.DevName = "SDVO Controller C";
    sdvo->d.SlaveAddr = 0x72;
  }
  sdvo->d.pI2CBus = i2cbus;
  sdvo->d.DriverPrivate.ptr = sdvo;
  sdvo->output_device = output_device;
  
  if (!xf86I2CDevInit(&sdvo->d)) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	       "Failed to initialize SDVO I2C device %s\n",
	       sdvo->d.DevName);
    xfree(sdvo);
    return NULL;
  }
  
  /* Set up our wrapper I2C bus for DDC.  It acts just like the regular I2C
   * bus, except that it does the control bus switch to DDC mode before every
   * Start.  While we only need to do it at Start after every Stop after a
   * Start, extra attempts should be harmless.
   */
  ddcbus = xf86CreateI2CBusRec();
  if (ddcbus == NULL) {
    xf86DestroyI2CDevRec(&sdvo->d, 0);
    xfree(sdvo);
    return NULL;
  }
  ddcbus->BusName = output_device == DVOB ? "SDVOB DDC" : "SDVOC DDC";
  ddcbus->scrnIndex = i2cbus->scrnIndex;
  ddcbus->I2CGetByte = I830SDVODDCI2CGetByte;
  ddcbus->I2CPutByte = I830SDVODDCI2CPutByte;
  ddcbus->I2CStart = I830SDVODDCI2CStart;
  ddcbus->I2CStop = I830SDVODDCI2CStop;
  ddcbus->I2CAddress = I830SDVODDCI2CAddress;
  ddcbus->DriverPrivate.ptr = sdvo;
  if (!xf86I2CBusInit(ddcbus)) {
    xf86DestroyI2CDevRec(&sdvo->d, 0);
    xfree(sdvo);
    return NULL;
  }
  
  pI830->output[output_index].pDDCBus = ddcbus;
    
  /* Read the regs to test if we can talk to the device */
  for (i = 0; i < 0x40; i++) {
    if (!sReadByte(sdvo, i, &ch[i])) {
      xf86DestroyI2CBusRec(pI830->output[output_index].pDDCBus, FALSE,
			   FALSE);
      xf86DestroyI2CDevRec(&sdvo->d, 0);
      xfree(sdvo);
      return NULL;
    }
  }
  
  pI830->output[output_index].sdvo_drv = sdvo;
  
  return sdvo;
}
