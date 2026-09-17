# This command tells the Pico SDK to enable USB stdio for our project
# and explicitly disable UART stdio to prevent conflicts.
pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
pico_enable_stdio_uart(${CMAKE_PROJECT_NAME} 0)