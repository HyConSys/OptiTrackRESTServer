# OptiTrackRESTServer

To install this application:

1. On Windows, install vcpkg and cpprestsdk. The steps for installation are given in this [link](https://github.com/Microsoft/cpprestsdk/wiki/Getting-Started-Tutorial). 

2. Install [Visual Studio Community Edition](https://visualstudio.microsoft.com/vs/community/) and make sure that the install includes CMake. 

3. Use the following commands in the terminal to prepare a build directory:

``` 
mkdir build
cd build
```

4. Then, to build the appliation, use the following commands:
``` 
cmake ..
cmake -build
``` 

5. When running the application, make sure it is being run as administrator and pass the config file (or use the run BATCH file "start_admin.bat").


