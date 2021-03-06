# EXE Proxy

This program serves as a proxy for another command line program. There 
are two versions of the program available:
 - simple (20 KB binary)
 - with JavaScript (270 KB binary)

EXE Proxy uses semantic versioning (http://semver.org/). The versions before
1.0 will change the interface incompatibly so please use an exact version 
number as dependency.

Usage scenarios:
 - enable access to a command line program under another (possibly shorter) name. 
 - store many copies of EXE proxy in one directory pointing to command line 
    utilities on the computer and
    adding only this directory to the PATH environment variable. This would avoid 
    reaching the maximum length for PATH (2048 characters) and making it very
    complicated while still be able to run all programs without specifying the 
    directory where they reside. 
 - make a GUI executable usable from the command line too. 
    Normally cmd.exe does not wait for
    GUI executables to end but returns immediately. EXE Proxy does not differentiate
    between GUI and non-GUI programs and always waits for the target process to end. 
 - launcher for Java or other programming languages that do not create executables
    by default.
 - writing Windows service in Java   

1. The simple version passes all arguments as-is to the target program. 
The return code of the target program will be returned as exit code by EXE Proxy.
The path and file name of the target executable are
stored directly as a resource in a copy of the EXE Proxy. During the start
EXE Proxy reads the resource string and uses it to find the target executable.
The resource string can either contain an absolute file name or a file name
without slashes or backslashes. In the latter case the target executable should
reside in the same directory as the EXE Proxy itself.

In order to change the target executable resource entry you could start the EXE
Proxy with the parameter "exeproxy-copy":

```bat
exeproxy.exe exeproxy-copy <output file name> <target executable name> [--copy-icons] [--copy-version] [--copy-manifest]
```

The second parameter should be the name of the output exe file where a copy of
the EXE Proxy will be stored. The third parameter should either contain an
absolute path to the target executable or the name of the target executable 
without slashes or backslashes if the target executable resides in the same
directory as the EXE Proxy.

If the parameter --copy-icons is present, all icons and icon groups are copied
making the executable icon look exactly as in the target executable.

If the parameter --copy-version is present, the version information is copied.

If the parameter --copy-manifest is present, the application XML manifest is copied.

2. The second version reads the JavaScript from the file with the same name as
the executable and the extension .js and executes it using the Duktape library
(https://duktape.org/). 

You can still use the "exeproxy-copy" command from 
above to embedd the icons and version information from the target executable, 
but it is not executed automatically (if not specified in the JavaScript).

You can create a Java based Windows service using JNA. See
https://github.com/java-native-access/jna/tree/master/contrib/ntservice for
more details. The Windows command line utility "sc.exe" can be used to register
and unregister the service.

In the JavaScript code are also defined:
  - os.totalmem() - total physical system memory in bytes
  - process.argv - executable parameters as an array of strings. The value at the index 0 is the name of the executable used on the command line.
  - process.argv0 - executable name including path
  - child_process.execSync(command) - executes a program and returns the exit code
  - process.exit(ec) - exits the program with the specified exit code  
  - process.loadJVM(options) - start Java Virtual Machine in the current process
     - options.jvmDLL - path to the jvm.dll
     - options.jvmOptions - JVM options (the same as arguments for java.exe) as an array of strings
        See https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html for more details.
  - process.javaCallMain(className, args) - execute "main" method in the specified Java class. The JVM must be loaded first.
     - className - name of the main class
     - args - argument for "public static void main(String[])" as an array of strings
 
Example JavaScript file:

```JavaScript
console.log('os.totalmem = ' + os.totalmem());
console.log('exe = ' + process.argv0);

console.log(process.argv.length);
for (var i = 0; i < process.argv.length; i++) {
    console.log("JS argument " + i + " = " + process.argv[i]);
}

process.loadJVM({
    jvmDLL: "C:\\Program Files (x86)\\Java\\jre7\\bin\\client\\jvm.dll", 
    jvmOptions: ["-Djava.class.path=C:\\Users\\IEUser\\Documents\\exe-proxy"],
});

process.javaCallMain("tests.Demo", ["first", "second"]);

var ec = child_process.execSync("C:\\msys64\\mingw32\\bin\\addr2line.exe params");
console.log('exit code = ' + ec);

process.exit(200);
```


