---
tags:
  - plugin
resource_link: "https://www.redguides.com/community/resources/mq2ice.1719/"
support_link: "https://www.redguides.com/community/threads/mq2ice.74227/"
repository: "https://github.com/RedGuides/MQ2Ice"
config: "MQ2Ice.ini"
authors: "dewey2461, MQ2MoveUtils authors"
tagline: "Helper for moving on ice"
---

# MQ2Ice

<!--desc-start-->
This plugin attempts to make moving on ice a little easier by automatically toggling run/walk and pausing nav/path/stick when it detects the player skidding.
<!--desc-end-->
How it works:

* When you are in first floor of ToFS near the ice the plugin forces you to walk
* When you are on ice and 'Skidding' the plugin will:
    * Pause Stick/Nav
    * Use forward/back/strife keys to halt motion
    * Unpause Stick/Nav when you have stopped 'Skidding'
* When you exit the ice area it will turn on anything it's turned off.

You can turn on/off each of the mitigation steps to see how they effect your movement.

## Commands

<a href="cmd-ice/">
{% 
  include-markdown "projects/mq2ice/cmd-ice.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2ice/cmd-ice.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2ice/cmd-ice.md') }}

## Settings

Example `MQ2Ice.ini`,

```ini
[Area]
1=825 140 212 -100 270 435 100
; The zone and area where there's ice. Format: [ZoneID] [Min X] [Min Y] [Min Z] [Max X] [Max Y] [Max Z] 
[Settings]
ON=1
; turn plugin on
Walk=1
; toggle run/walk on ice
Nav=1
; toggle pause nav on ice
Path=1
; toggle pause advpath on ice
Stick=1
; toggle pause /stick on ice
```

In the `[Area]` section, the syntax is as follows: `[Unique ID] = [ZoneID] [Min X] [Min Y] [Min Z] [Max X] [Max Y] [Max Z]` 

The zone ID in the example is 825, meaning Towers of Frozen Shadows.
