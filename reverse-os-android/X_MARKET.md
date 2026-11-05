# X MARKET · interfaz integrada para REVERSE Workbench 2.1.1

## Entrega

X MARKET se integra como aplicación de **REVERSE Workbench** bajo el identificador interno xmarket. Se muestra en Inicio → Aplicaciones (con búsqueda), accesos del escritorio, panel Aplicaciones y barra de tareas al lado de BBY Q. Abre una sola ventana por espacio; abrir desde Inicio no inicia una Activity antigua ni sustituye REVERSE OS.

La interfaz funciona offline y no instala aplicaciones Android independientes. Su CSS y JavaScript se cargan como assets locales del WebView protegido que ya utiliza REVERSE Workbench.

Pantallas:

1. **Mercado:** selector SPY/QQQ/AAPL/NVDA, 1/5/20 días; todos los precios, variaciones y señales permanecen sin datos hasta que exista una fuente verificada. No se trazan curvas de mercados ficticias.
2. **Laplace:** explicación gráfica e interactiva de la distinción entre exactitud de cálculo y conocimiento de eventos futuros. Los dos escenarios están etiquetados como **sintéticos**, con números provenientes del experimento XI externo.
3. **Experimentos:** tres fichas sobre los cálculos sintéticos verificados en la campaña XI de REVERSE-PC PR #27.
4. **Configuración:** conexiones inexistentes declaradas y hoja de ruta, sin controles de trading fingidos.

## Qué significa «instalar en Inicio»

El ejecutable Android que contiene la app es **REVERSE Workbench**, paquete com.reversepc.workbench. X MARKET aparece como app integrada de ese escritorio, al igual que el acceso BBY Q. **No es todavía un ejecutable independiente X-MARKET.rxe**: el RXE del experimento XI (PR #27) no se ejecuta al abrir la interfaz porque la solicitud actual es solo diseñar las pantallas. Nada se ha atribuido falsamente al núcleo.

Actualizar la APK aumenta versionCode de 210 a 211 y conserva paquete y firma de desarrollo; los documentos, preferencias, ventanas y checkpoints de la versión instalada no deben borrarse. No desinstalar antes de actualizar.

## Evidencia matemática mostrada

La regla del ejemplo es P(t+1) = P(t)+7+3t, con P(0)=10000 céntimos, durante 40 iteraciones. Pronóstico sin evento: 12620; después de un evento exógeno no observado de +250 céntimos: 12870. El error de la predicción original fue +250. Ambas cifras y la equivalencia de ejecución XI/RXE se verificaron en GitHub Actions, en el PR #27.

El gráfico deriva únicamente esas ecuaciones sintéticas. No usa cotizaciones de bolsa y no es un modelo de un proceso bursátil. No se conecta a redes ni al runtime XI al abrir la ventana. El estado del núcleo RVPC2 no debe variar por abrir X MARKET o tocar la interfaz.

## Persistencia y verificación

desktop.js registra la aplicación, el acceso y los parámetros validados de la ventana. control-center.js la incluye en la barra y Centro de control. x-market.js genera el contenido, botones y escenarios; x-market.css define el diseño. index.html carga ambos assets dentro de Workbench. El resto de apps y sus ejecutables permanecen intactos.

Pruebas:

- node reverse-os-android/tests/x-market-ui-test.js verifica componentes, botones, registro en Inicio, selección, escenarios y que la representación de mercado no inventa precios.
- node --check de los módulos originales y nuevos.
- WorkbenchTestRunner en Android comprueba apertura desde Inicio, salto a Laplace, etiqueta sintética, navegación por el dock y checkpoint RVPC2 intacto.
- El workflow Android crea una APK firmada con clave de desarrollo; no reemplaza pruebas en el Samsung A16 físico.

## Siguiente fase, sin ejecutar en esta entrega

Definir un contrato de proveedor OHLCV, reproducibilidad temporal estricta, predicciones archivadas antes del resultado y un adaptador de XI para operar con datos observables, sin afirmar precisión perfecta ni que el experimento de Laplace conozca eventos futuros.
