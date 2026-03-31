

# RAK-GAN Drone Motor Test 

This code example was created for testing the RAK-GAN with the MIAT8318 100KV motor.

 <img src="images/rak_gan_uav_motor.jpg" style="zoom:100%;" />

## Requirements

- [ModusToolbox® software](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/) v3.7 or later (tested with v3.7).
- Motor Suite 2.8.1 GUI.
- The latest hardware release: RAK-GAN Rev. 1.
- At least 500W DC Power Supply.
- At least 2.5 mm2 (14 AWG) Power Cables.

## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v14.2.1 (`GCC_ARM`) - Default value of `TOOLCHAIN`

## Using the code example

Create the project and open it using one of the following:

<details><summary><b>In Eclipse IDE for ModusToolbox&trade; software</b></summary>


1. Click the **New Application** link in the **Quick Panel** (or, use **File** > **New** > **ModusToolbox&trade; Application**). This launches the [Project Creator](https://www.infineon.com/ModusToolboxProjectCreator) tool.

2. Pick a kit supported by the code example from the list shown in the **Project Creator - Choose Board Support Package (BSP)** dialogue.

   When you select a supported kit, the example is reconfigured automatically to work with the kit. To work with a different supported kit later, use the [Library Manager](https://www.infineon.com/ModusToolboxLibraryManager) to choose the BSP for the supported kit. You can use the Library Manager to select or update the BSP and firmware libraries used in this application. To access the Library Manager, click the link from the **Quick Panel**.

   You can also just start the application creation process again and select a different kit.

   If you want to use the application for a kit not listed here, you may need to update the source files. If the kit does not have the required resources, the application may not work.

3. In the **Project Creator - Select Application** dialogue, choose the example by enabling the checkbox.

4. (Optional) Change the suggested **New Application Name**.

5. The **Application(s) Root Path** defaults to the Eclipse workspace which is usually the desired location for the application. If you want to store the application in a different location, you can change the *Application(s) Root Path* value. Applications that share libraries should be in the same root path.

6. Click **Create** to complete the application creation process.

For more details, see the [Eclipse IDE for ModusToolbox&trade; software user guide](https://www.infineon.com/MTBEclipseIDEUserGuide) (locally available at *{ModusToolbox&trade; software install directory}/docs_{version}/mt_ide_user_guide.pdf*).

</details>

### Operation

The motor can be controlled in two ways: using the Motor Suite GUI or simply by turning the potentiometer POT2. All the parameters are adjusted to work with the MIAT8318 KV100 motor and 19x10 propeller. The PID has fixed values in the ParamConfig.c file:

```
// Speed Control Parameters:
#if defined(CTRL_METHOD_RFO)
if(!params_ptr->autocal_disable.speed_control) /*Skip the calculation if this bit is set*/
{
	//params_ptr->ctrl.speed.kp = ((8.0f / 3.0f) / (POW_TWO(params_ptr->motor.P) * params_ptr->motor.lam)) * params_ptr-		  		>mech.inertia * params_ptr->ctrl.speed.bw; // [A/(Ra/sec-elec)]
	//params_ptr->ctrl.speed.ki = ((8.0f / 3.0f) / (POW_TWO(params_ptr->motor.P) * params_ptr->motor.lam)) * params_ptr-			>mech.viscous * params_ptr->ctrl.speed.bw * params_ptr->ctrl.speed.ki_multiple; // [A/(Ra/sec-elec).(Ra/sec)]

	/*MIAT 8318 100KV with mounted 19x10 propeller*/
	params_ptr->ctrl.speed.kp = 0.005079614f;
	params_ptr->ctrl.speed.ki = 0.01179f;
}
```

The motor parameters, along with many other important settings, are in ParamConfig.h

```
/*******Motor*******/
#define MOTOR_POLE                                 (42.0f)                      /*[],  motor poles*/
#define MOTOR_LQ                                   (50.0E-6f)                   /*[H], Stator q-axis inductance*/
#define MOTOR_LD                                   (50.0E-6f)                   /*[H], Stator d-axis inductance*/
#define MOTOR_I_AM                                 (0.05)                       /*[Wb],  Rotor flux linkage*/
#define MOTOR_R                                    (0.055)                      /*{Ohm],  stator resistance*/
#define MOTOR_TORQUE_MAX                           (6.2f)                       /*[Nm],  maximum torque*/
#define MOTOR_CURRENT_PEAK                         (68)                         /*[A],  peak current rating*/
#define MOTOR_CURRENT_CONT                         (58)                         /*[A],  continuous current rating*/
#define MOTOR_ID_MAX                               (68)                         /*[A], maximum d-axis current*/
#define MOTOR_VOLTAGE                              (48.0f)                      /*[V], motor voltage*/
#define MOTOR_NORM_SPEED                           (4200.0f)                    /*[RPM], nominal speed*/
#define MOTOR_MAX_SPEED                            (5000.0f)                    /*[RPM],  maximum no load speed*/
#if defined(CTRL_METHOD_SFO)
#define MOTOR_MTPV_TORQUE_MARGIN                   (90.0f)                      /*[%],  MTPV torque margin*/
```

To control the motor using Motor Suite, connect the power supply to RAK-GAN, connect the USB connector to the CC1 board and PC and click on the ModusToolbox™ Motor Suite icon in the Quick Panel:

<img src="images/suite_start.png" style="zoom:100%;" />

On program entry, you may select the firmware and library versions. Please do as it is shown below and click on "Open Project":

<img src="images/suite_setup.png" style="zoom:100%;" />

The potentiometer control is always on by default. Switch it off and slowly increase the throttle "Target Set". In this case, the motor will start spinning at 4% of maximum speed in Current Open Loop mode. At 8% of maximum speed, the observer will take over, and FOC will be engaged. 

<img src="images/motor_suite_gui.jpg" style="zoom:100%;" />

### Debugging

If you have successfully imported the example, the debug configurations are already prepared to use with the JLink. Open the ModusToolbox™ perspective and find the Quick Panel. Click the desired debug launch configuration, then wait for programming to complete and debugging to start.

<img src="images/debug_start.png" style="zoom:100%;" />

## Legal Disclaimer

The evaluation board including the software is for testing purposes only and, because it has limited functions and limited resilience, is not suitable for permanent use under real conditions. If the evaluation board is nevertheless used under real conditions, this is done at one’s responsibility; any liability of Rutronik is insofar excluded. 

<img src="images/rutronik_origin_kaunas.png" style="zoom:50%;" />



