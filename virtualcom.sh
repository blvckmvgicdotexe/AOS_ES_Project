stty -F /dev/ttyACM0 115200 raw -clocal -echo icrnl
socat file:/dev/ttyACM0 stdio
