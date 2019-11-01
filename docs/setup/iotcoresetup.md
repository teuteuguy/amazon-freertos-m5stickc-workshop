---
permalink: /setup/iotcoresetup
---
# Create your AWS IoT Thing

## Creating a thing requires 3 main steps:

1. Create a thing for the device in IoT Core
2. Create a certificate and attach it to the thing
3. Create a Policy and attach it to the certificate

## Create a thing for your IoT device in AWS IoT Core

Navigate to IoT Core to enter AWS IoT services
![IoT Core 0](../assets/images/iotcore-0.png)

Follow the below 3 steps to register your thing:

1. Before you create a thing, please select the region. please select **us-east-1**
2. To Create a thing, navigate to Manager → Things
3. Click Create to start creation and registration of your device

![IoT Core 1](../assets/images/iotcore-1.png)

If this is the very first time you create a device, the following page will appear, please click “**Register a thing**” to get started

![IoT Core 2](../assets/images/iotcore-2.png)

In Creating AWS IoT Things page, choose **“create single thing“**

![IoT Core 3](../assets/images/iotcore-3.png)

Input a device name of your choice and click **“Next”**

![IoT Core 4](../assets/images/iotcore-4.png)

## Create a certificate and attach it to the thing

Choose One-click certificate creation

![IoT Core 5](../assets/images/iotcore-5.png)

If this is the first time you create a thing in AWS IoT, you may not have a policy for the thing. you can click “Register Thing” and attach policy later.

![IoT Core 6](../assets/images/iotcore-6.png)

So let’s see exactly what was generated.

* a thing named “[YOUR CHOSEN DEVICE NAME]”

![IoT Core 7](../assets/images/iotcore-7.png)

* A certificate that is attached to the thing

![IoT Core 8](../assets/images/iotcore-8.png)

* certificates that you’ve downloaded in your local PC

![IoT Core 9](../assets/images/iotcore-9.png)


> Note - Do not lose this zip file, it contains your private key file which cannot be retrieved again.

## Create a Policy and attach it to the certificate

Next step we will create a policy to the certificate we’ve created in the previous step
![IoT Core 10](../assets/images/iotcore-10.png)
input the policy name and enter advanced mode to past the policies
![IoT Core 11](../assets/images/iotcore-11.png)
copy the following policy statement into the statements


```bash
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:Subscribe",
        "iot:Connect",
        "iot:Receive"
      ],
      "Resource": [
        "*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "greengrass:*"
      ],
      "Resource": [
        "*"
      ]
    }
  ]
}
```

copy the above policy statement and create
![IoT Core 12](../assets/images/iotcore-12.png)

After creation, in the IoT Core console, navigate to **“Secure → Policies**“ your policy should be there 
![IoT Core 13](../assets/images/iotcore-13.png)

navigate to **“Secure → Certificates**“ 
![IoT Core 14](../assets/images/iotcore-14.png)

In the certificate page click **“Actions → Attach policy”**, 
![IoT Core 15](../assets/images/iotcore-15.png)

Select policy and attach
![IoT Core 16](../assets/images/iotcore-16.png)

Now, check the polices of the certificate, and the policy you’ve added should be there
![IoT Core 17](../assets/images/iotcore-17.png)


## Next Step

[Compile the Amazon FreeRTOS workshop code](./compileworkshopcode.html)

