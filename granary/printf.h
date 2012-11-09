/*
 * printk.h
 *
 *  Created on: 2012-11-09
 *      Author: pag
 *     Version: $Id$
 */

#ifndef Granary_PRINTK_H_
#define Granary_PRINTK_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int (**kernel_printf)(const char *, ...);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace granary {
    extern int (*printf)(const char *, ...);
}
#endif

#endif /* Granary_PRINTK_H_ */
