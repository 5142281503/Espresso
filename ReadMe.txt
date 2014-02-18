This Console application runs in a command prompt on Windows Operating Systems.

Note, you may be able to add it as an additional COFEE tool by Clicking "Add Tool"; 
see page 22 of the "User Guide for COFEE v112.pdf".

Email questions and suggestions to: marioc@computer.org


Hints:

How do we run it?

- Use a Command prompt (cmd.exe)
  Runs in either administrator command prompt, or not.
  If you can open the Windows command prompt as administrator, use that as this will gather additional data.
  If the currently logged on user is of Admin-class, no password is required even if UAC is on.


How should it be executed?

- The program needs to be run using the USB path. 
  The reason is to avoid accidentally executing another "espresso.exe" in the %PATH% on the target machine.


Why can't it be executed by double-clicking it in Explorer?

- It can, and it will run normally. The reason why this is not recommended is because it
  causes less disk IO if run directly from the USB folder in a command prompt.
  In addition, the Explorer shell on the target PC may be directed to perform operations both before opening, and after closing,
  which is something we want to avoid.


Where to run it from:

- Run it from the USB device (eg: E:\espresso\espresso.exe).
  (Change the current directory to "E:" in the command prompt by typing "E:" <Enter>)


Where does it place the generated CABinet files?

- Where the espresso.exe application is run from. 
  Using the example above, the results will end up in E:\espresso\XXX-EspressoResults1.CAB.


What if the file E:\espresso\XXX-EspressoResults1.CAB already exists?

- XXX-Results1.CAB should not exist since you should run it from a clean USB device.
  If any of the files already exist they are overwritten.
  
  
What if one of the files in the path is a NT reparse point, will it ignore it?

- Yes it will. This avoids endless loops.


What if a folder or filename contains UNICODE characters?

- Yes, folder and filenames will be handled properly.


Does executing this create any Windows Events Log entries?

- No


What if there are more than one Windows account, will it read the data from the other users?

- No. There are two reasons for this. 1, if the drive is encrypted it will read rubbish; and 2,
  this can take too long and use up too much space.


What files are the easiest, or the low hanging fruit, to verify in the archives?

- ComputerName_UserName_IE Temp Low Files_Date_EspressoResults1.CAB	=> index.dat	=> IE history
- ComputerName_UserName_IE Temp Files_Date_EspressoResults1.CAB		=> index.dat	=> IE history
- ComputerName_UserName_Windows Live Mail_Date_EspressoResults1.CAB	=> *.eml		=> Emails
- ComputerName_UserName_Windows Live Mail_Date_EspressoResults1.CAB	=> *.oeaccount	=> Email/News Account Info (gmail, POP3)
- ComputerName_UserName_Boinc_Date_EspressoResults1.CAB				=> account_XXX.xml			=> BOINC
- ComputerName_UserName_jungledisk_Date_EspressoResults1.CAB		=> jungledisk-settings.xml	=> Amazon Account Info
- ComputerName_UserName_FireFox Cookies_Date_EspressoResults1.CAB	=> cookies.sqlite			=> FF Cookies


How big should the USB key be?

- The bigger it is, the better, there is less chance of running out of space.


I see some errors in the output, should I be worried?

- No, that is normal, some files cannot be opened when these are in use by another application.


What was this tested on?

- Validated on Vista 64bits, Windows 7 64bits, Windows 7 32-bits, but only the English OS at this time.
  Generated Cab files can be opened with either the Windows built-in tool, WinRAR, or WinZip.