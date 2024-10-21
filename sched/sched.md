# sched

## Informe

 <!-- Guía para tamaños de las imágenes: 1 línea de texto son 20px. Por ejemplo, para 4 líneas de texto se tendría 4*20 = 80 => height="80px" -->

### Visualización del cambio de contexto

Se comienza el análisis de ejecución justo antes de ingresar a la función `context_switch`:

<img src="./docs/imgs/context_switch_4.png" height="80px">

Se muestra el estado del `struct Trapframe` de un environment de tipo `hello.c`:

<img src="./docs/imgs/context_switch_1.png" height="100px">

El code segment (`tf_cs`) posee en los 2 lowest bits el ring con el que se ejecutará el env:

<img src="./docs/imgs/context_switch_2.png" height="40px">

En binario, 27 es representado como `11011`, y los últimos dos bits (`11`) equivalen al CPL (Current Privilege Level), o ring, 3.

Se muestran todos los valores del `struct Env`:

<img src="./docs/imgs/context_switch_3.png" height="140px">

Se avanza la ejecución (siempre se hará de a una línea a la vez), para ejecutar:

<img src="./docs/imgs/context_switch_8.png" height="40px">

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_9.png" height="340px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_10.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    mov (%esp), %esp
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_11.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_12.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    popal
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_13.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_14.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    pop %es
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_15.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_16.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    pop %ds
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_17.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_18.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    add $8, %esp
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_19.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_20.png" height="160px">

Se avanza la ejecución para ejecutar:

```
    iret
```

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_21.png" height="420px">

Se muestran los últimos 28 valores (de 32 bits, en formato hexadecimal) del stack:

<img src="./docs/imgs/context_switch_22.png" height="160px">

Se avanza la ejecución, por lo que se ejecutó `iret` y se transfirió el control de ejecución al programa de usuario.

<img src="./docs/imgs/context_switch_23.png" height="60px">

Se muestran los valores almacenados en los registros:

<img src="./docs/imgs/context_switch_24.png" height="460px">

### Verificación de las syscalls funcionando

Se pone un breakpoint en `gdb` en la función `sys_cputs`, syscall cuya definición se encuentra en `kern/syscall.c`. Esta syscall es una parte esencial del funcionamiento de la función `cprintf`, de funcionamiento análogo al printf de la biblioteca estándar de C.

<img src="./docs/imgs/syscall_verification_1.png" height="260px">

<img src="./docs/imgs/syscall_verification_2.png" height="50px">

Utilizando el programa de usuario `hello.c`, que sólo imprime por pantalla dos líneas de texto, se verificará empíricamente que el cambio user-kernel y kernel-use:

<img src="./docs/imgs/syscall_verification_3.png" height="180px">

Se deja pasar una ejecución de la syscall y se observa el output del programa como `hello, world` en la pantalla:

<img src="./docs/imgs/syscall_verification_4.png" height="380px">

Se para el programa en el segundo `cprintf`, y en gdb se observa que a la función `sys_cputs` efectivamente llega el string argumento:

<img src="./docs/imgs/syscall_verification_5.png" height="130px">
<img src="./docs/imgs/syscall_verification_6.png" height="100px">

Continuando la ejecución, se imprime el string mencionado como output del programa:

<img src="./docs/imgs/syscall_verification_7.png" height="80px">

La ejecución finaliza exitosamente:

<img src="./docs/imgs/syscall_verification_8.png" height="100px">

### Implementación de scheduler con prioridades

Programas de test: `prioritytest1.c`
