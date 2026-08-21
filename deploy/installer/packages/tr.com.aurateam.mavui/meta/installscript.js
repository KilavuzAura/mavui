function Component()
{

}

Component.prototype.createOperations = function()
{
    try {
        component.createOperations();
    } catch (e) {
        console.log(e);
    }

    if (systemInfo.productType === "windows") {
        component.addElevatedOperation("Execute", "msiexec", ["/i", "@TargetDir@/driver.msi", "/qn"]);

        // Executable and shortcut names come from the installer config (config.xml.in
        // fills them from CMAKE_PROJECT_NAME), not from a hardcoded QGroundControl.
        component.addOperation("CreateShortcut", "@TargetDir@/bin/@ProductName@.exe", "@StartMenuDir@/@ProductName@.lnk");
        component.addOperation("CreateShortcut", "@TargetDir@/bin/@ProductName@.exe", "@DesktopDir@/@ProductName@.lnk");
    }
}
