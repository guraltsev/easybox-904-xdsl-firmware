/******************************************************************************
**
** FILE NAME    : ifxmips_gpio_amazon_se.h
** PROJECT      : IFX UEIP
** MODULES      : GPIO
**
** DATE         : 22 May 2009
** AUTHOR       : Xu Liang
** DESCRIPTION  : IFX GPIO driver header file for Amazon-SE
** COPYRIGHT    :       Copyright (c) 2006
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 22 May 2009   Xu Liang        UEIP
*********************************************************************/



#ifndef IFXMIPS_GPIO_AMAZON_SE_H
#define IFXMIPS_GPIO_AMAZON_SE_H



static ifx_gpio_port_t g_gpio_port_priv[2] = {
    {
        .reg = {
            .gpio_out     = (volatile unsigned long *)IFX_GPIO_P0_OUT,
            .gpio_in      = (volatile unsigned long *)IFX_GPIO_P0_IN,
            .gpio_dir     = (volatile unsigned long *)IFX_GPIO_P0_DIR,
            .gpio_altsel0 = (volatile unsigned long *)IFX_GPIO_P0_ALTSEL0,
            .gpio_altsel1 = (volatile unsigned long *)IFX_GPIO_P0_ALTSEL1,
            .gpio_od      = (volatile unsigned long *)IFX_GPIO_P0_OD,
            .gpio_stoff   = (volatile unsigned long *)IFX_GPIO_P0_STOFF,
            .gpio_pudsel  = (volatile unsigned long *)IFX_GPIO_P0_PUDSEL,
            .gpio_puden   = (volatile unsigned long *)IFX_GPIO_P0_PUDEN,
        },
        .pin_num = IFX_GPIO_PIN_NUMBER_PER_PORT,
    },
    {
        .reg = {
            .gpio_out     = (volatile unsigned long *)IFX_GPIO_P1_OUT,
            .gpio_in      = (volatile unsigned long *)IFX_GPIO_P1_IN,
            .gpio_dir     = (volatile unsigned long *)IFX_GPIO_P1_DIR,
            .gpio_altsel0 = (volatile unsigned long *)IFX_GPIO_P1_ALTSEL0,
            .gpio_altsel1 = (volatile unsigned long *)IFX_GPIO_P1_ALTSEL1,
            .gpio_od      = (volatile unsigned long *)IFX_GPIO_P1_OD,
            .gpio_stoff   = (volatile unsigned long *)IFX_GPIO_P1_STOFF,
            .gpio_pudsel  = (volatile unsigned long *)IFX_GPIO_P1_PUDSEL,
            .gpio_puden   = (volatile unsigned long *)IFX_GPIO_P1_PUDEN,
        },
        .pin_num = IFX_GPIO_PIN_NUMBER_PER_PORT,
    }
};

//  function must be declared as "pure_initcall" which be invoked as early as possible
//  no any functions are supposed to be called in this function
static int init_gpio_port_priv(void)
{
    int i, j;

    for ( i = 0; i < NUM_ENTITY(g_gpio_port_priv); i++ ) {
        for ( j = 0; j < IFX_GPIO_PIN_NUMBER_PER_PORT; j++ )
            g_gpio_port_priv[i].pin_status[j] = IFX_GPIO_PIN_AVAILABLE;
    }

    return 0;
}
pure_initcall(init_gpio_port_priv);



#endif  //  IFXMIPS_GPIO_AMAZON_SE_H
