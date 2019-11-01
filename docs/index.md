---
layout: default
---

Welcome to this Amazon FreeRTOS workshop using the [M5StickC](https://docs.m5stack.com/#/en/core/m5stickc)


{% for p in site.posts reversed %}
## [{{ p.title }}]({{ p.name | absolute_url }})
{{ p.excerpt }}
{% endfor %}


## Disclaimer
The following workshop material including documentation and code, is provided as is. You may incur AWS service costs for using the different resources outlined in the labs. Material is provided AS IS and is to be used at your own discretion. The author will not be responsible for any issues you may run into by using this material. 

If you have any feedback, suggestions, please use the issues section.