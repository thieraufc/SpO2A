README.md
---------
The code in this repository ports code from the following repos:
- https://github.com/MaximIntegratedRefDesTeam/RD117_ARDUINO
- https://github.com/aromring/MAX30102_by_RF

Files used in the build:
- RD117piA
  - max30102pi.h
  - max30102pi.cpp
  - algorithm2.h
  - algorithm2.cpp
  - algorithm2_in_RF.h
  - algorithm2_in_RF.cpp

compile command:
  gcc -Wall -pthread -o RD117piA RD117piA.cpp -lpigpio -lrt

  Note: you will get 6 warnings about unused variables.

execute command:
  sudo ./RD117piA


