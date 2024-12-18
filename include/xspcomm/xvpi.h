#ifndef __xspcomm_xvpi__
#define __xspcomm_xvpi__

/*
*  VPI defines from verilator/include/vltstd/vpi_user.h
*/

typedef int              PLI_INT32;
typedef unsigned int     PLI_UINT32;
typedef short            PLI_INT16;
typedef unsigned short   PLI_UINT16;
typedef char             PLI_BYTE8;
typedef unsigned char    PLI_UBYTE8;

typedef PLI_UINT32 *vpiHandle;

#define vpiType                1   /* type of object */
#define vpiName                2   /* local name of object */
#define vpiFullName            3   /* full hierarchical name */
#define vpiSize                4   /* size of gate, net, port, etc. */
#define vpiNet                36   /* scalar or vector net */
#define vpiReg                48   /* scalar or vector reg */

#define vpiNoDelay             1
#define vpiInertialDelay       2
#define vpiTransportDelay      3
#define vpiPureTransportDelay  4
#define vpiForceFlag           5
#define vpiReleaseFlag         6


typedef struct t_vpi_time
{
  PLI_INT32  type;               /* [vpiScaledRealTime, vpiSimTime,
                                     vpiSuppressTime] */
  PLI_UINT32 high, low;          /* for vpiSimTime */
  double     real;               /* for vpiScaledRealTime */
} s_vpi_time, *p_vpi_time;


typedef struct t_vpi_vecval
{
  /* following fields are repeated enough times to contain vector */
  PLI_UINT32 aval, bval;          /* bit encoding: ab: 00=0, 10=1, 11=X, 01=Z */
} s_vpi_vecval, *p_vpi_vecval;


typedef struct t_vpi_strengthval
{
  PLI_INT32 logic;               /* vpi[0,1,X,Z] */
  PLI_INT32 s0, s1;              /* refer to strength coding below */
} s_vpi_strengthval, *p_vpi_strengthval;


typedef struct t_vpi_value
{
  PLI_INT32 format; /* vpi[[Bin,Oct,Dec,Hex]Str,Scalar,Int,Real,String,
                           Vector,Strength,Suppress,Time,ObjType]Val */
  union
    {
      PLI_BYTE8                *str;       /* string value */
      PLI_INT32                 scalar;    /* vpi[0,1,X,Z] */
      PLI_INT32                 integer;   /* integer value */
      double                    real;      /* real value */
      struct t_vpi_time        *time;      /* time value */
      struct t_vpi_vecval      *vector;    /* vector value */
      struct t_vpi_strengthval *strength;  /* strength value */
      PLI_BYTE8                *misc;      /* ...other */
    } value;
} s_vpi_value, *p_vpi_value;


typedef PLI_INT32    (*func_vpi_get)           (PLI_INT32   property,
                                                vpiHandle   object);

typedef void         (*func_vpi_get_value)     (vpiHandle   expr,
                                                p_vpi_value value_p);

typedef vpiHandle    (*func_vpi_put_value)     (vpiHandle   object,
                                                p_vpi_value value_p,
                                                p_vpi_time  time_p,
                                                PLI_INT32   flags);

typedef PLI_BYTE8    (*func_vpi_get_str)       (PLI_INT32   property,
                                                vpiHandle   object);

#endif
