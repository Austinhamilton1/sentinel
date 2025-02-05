# sentinel

The sentinel package is an application for developers. It allows for a more seamless experience when developing code - specifically for coding on a server. Sentinal will listen for changes to source code on a source machine and automatically push those changes to the connected server. This can be done through different protocols (e.g., FTP, SFTP, SSH) depending on the environment. Hopefully sentinel can make the development of code a little more streamlined for you. Happy coding!

# TODO
* ~~Design dev folder structure~~
* ~~Create configuration script~~ 
* ~~Create makefile(s)~~
    * ~~Build~~
    * ~~Install~~
	* ~~Uninstall~~
* ~~Create way to store state of source code~~
* ~~Create main event loop~~
* Create way to determine which type of connection
	* Same physical filesystem
	* FTP
	* SFTP
	* SSH
* Create way to connect to remote destination
    * ~~Same physical filesystem~~ 
    * FTP
	* SFTP
	* SSH
* Update destination when there is a change to source code
	* ~~Same physical filesystem~~
	* FTP
	* SFTP
	* SSH
