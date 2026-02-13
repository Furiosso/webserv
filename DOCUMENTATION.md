## Resumen
“Fuerza cada lectura/escritura en memoria” significa que el compilador no mantendrá `g_running` en un registro de la CPU; cada acceso se realizará contra la memoria principal (RAM/caché). Sin `volatile`, el bucle `while (g_running)` podría cachear el valor en un registro y nunca notar que el manejador de señales lo cambió a 0. Con `volatile sig_atomic_t`, cada lectura en `Server::start` y cada escritura en `handleSignal` van a memoria, garantizando visibilidad inmediata y atomicidad [1](#2-0) [2](#2-1) [3](#2-2) .

---

## ¿Dónde “está” una variable si no está en memoria?
- **En un registro de la CPU**: durante la optimización, el compilador puede mantener una variable en un registro rápido (ej. `eax`, `rdi`) para evitar accesos a memoria en bucles ajustados.
- **En memoria principal (RAM)**: la ubicación normal de variables globales como `g_running`. Los accesos pueden pasar por la caché de CPU, pero son visibles para otros hilos/interrupciones.

## ¿Qué hace `volatile` aquí?
- Impide que el compilador asuma que `g_running` no cambia externamente.
- Obliga a que cada lectura/escritura genere una instrucción de carga/store desde/hacia memoria, en lugar de reutilizar un valor en registro.
- En el bucle `while (g_running)` de `Server::start`, sin `volatile` el compilador podría cargar `g_running` una vez y nunca volver a leerla, ignorando que `handleSignal` la puso a 0 [3](#2-2) .

## ¿Por qué `sig_atomic_t` también importa?
- Garantiza que la operación de lectura o escritura es atómica (indivisible) a nivel de hardware, incluso si una señal interrumpe en medio del acceso.
- En este código, `handleSignal` escribe `g_running = 0` de forma atómica, y el bucle principal lo lee atómicamente [4](#2-3) [3](#2-2) .

## Ejemplo en este códigobase
- Declaración: `extern volatile sig_atomic_t g_running;` en `library.hpp` [1](#2-0) .
- Definición: `volatile sig_atomic_t g_running = 1;` en `signalUtils.cpp` [5](#2-4) .
- Escritura asíncrona: `handleSignal` pone `g_running = 0` al recibir SIGINT/SIGTERM [4](#2-3) .
- Lectura continua: `while (g_running)` en `Server::start` se detiene en cuanto ve el cambio [3](#2-2) .

## Notas
- `volatile` no implica atomicidad por sí solo; por eso se combina con `sig_atomic_t` para comunicación segura entre manejador de señales y el bucle principal.
- Este patrón (`volatile sig_atomic_t`) es el forma portable en C/C++ para señalizar desde un manejador de señales al resto del programa.

Wiki pages you might want to explore:
- [Overview (jfercode/42-Webserv)](/wiki/jfercode/42-Webserv#1)

Wiki pages you might want to explore:
- [Overview (jfercode/42-Webserv)](/wiki/jfercode/42-Webserv#1)
- [Architecture Overview (jfercode/42-Webserv)](/wiki/jfercode/42-Webserv#1.2)

### Citations

**File:** inc/library.hpp (L114-114)
```text
extern volatile sig_atomic_t g_running;
```

**File:** src/utils/signalUtils.cpp (L15-21)
```cpp
volatile sig_atomic_t g_running = 1;

void handleSignal(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
		g_running = 0; // tell main loop to stop
}
```

**File:** src/core/server/Server.cpp (L576-576)
```cpp
	while (g_running)
```
