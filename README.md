shim-logger
===========

A simple Windows app which can replace another one and log its command line inputs and pass through the command.

In order to use this, you need to rename the binary to the same name as the executable you would like to intercept, and set an environment variable with the same name plus "_cmd" that points to the real program.  For example:

# Set up the shim-logger
mv ConsoleApplication1.exe cl.exe
export cl_cmd="C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\VC\\bin\\cl.exe"
# Run whatever command that ultimately runs the executable you would like to intercept
make

You can also poison a specific command line argument.  For example, to catch a case where we pass -MD to the compiler, you can set the following environmet variable before invoking the command:

export cl_poison=-MD
