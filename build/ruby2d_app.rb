require_relative 'commands.rb'
require 'byebug'

state = 0

c = Commands.new()
d1 = Dino.new("dinos/dino1.png", 1)
d2 = Dino.new("dinos/dino2.png", 2)
d3 = Dino.new("dinos/dino3.png", 3)
d4 = Dino.new("dinos/dino4.png", 4)
title = Text.new('dino timer', x: 180, y: 30, size: 60, color: 'black', rotate: 0, z: 10)
$timer_text = Text.new('00:00:00', x:  221, y: 131, size: 60, color: 'black', rotate: 0, z: 10)

### setup game BG
bg_1 = Image.new('UI/bg01.png', x: 0, y: 0, width: 1920 / 2, height: 1080 / 2, rotate: 0, z: 5)
uibox_1 = Image.new('UI/UIBox1.png', x: 100, y: 100, width: 1920 / 4, height: 1080 / 4, rotate: 0, z: 5)
uibox_3greens = Image.new('UI/UIBox3Greens.png', x: 100, y: 100, width: 1920 / 4, height: 1080 / 4, rotate: 0, z: 5)
uibox_3texts = Image.new('UI/UIBox3Texts.png', x: 100, y: 100, width: 1920 / 4, height: 1080 / 4, rotate: 0, z: 5)
#x_left, x_right, y_bot, y_top
b_0 = Button.new(198, 232, 293, 329, "num", 0)
b_1 = Button.new(242, 278, 293, 329, "num", 1)
b_2 = Button.new(289, 325, 293, 329, "num", 2)
b_3 = Button.new(335, 372, 293, 329, "num", 3)
b_4 = Button.new(383, 419, 293, 329, "num", 4)
b_clear = Button.new(431, 488, 293, 329, "clear", 0)
b_5 = Button.new(198, 232, 246, 282, "num", 5)
b_6 = Button.new(242, 278, 246, 282, "num", 6)
b_7 = Button.new(289, 325, 246, 282, "num", 7)
b_8 = Button.new(335, 372, 246, 282, "num", 8)
b_9 = Button.new(383, 419, 246, 282, "num", 9)
b_set = Button.new(431, 488, 246, 282, "set", 0)
buttons = [b_0, b_1, b_2, b_3, b_4, b_5, b_6, b_7, b_8, b_9, b_set, b_clear]


tick = 0
$my_timer = Timer.new
# timer_nums = [0, 0, 0, 0, 0, 0]
timer_stack = 0

update do
    if tick % 60 == 0
      set background: 'random'
    end
    tick += 1
end

on :mouse_down do |event|
    puts event.x, event.y

    ## state = 0 :: menu
    if state == 0
        buttons.each_with_index do |button, index|
            check_timer_button(button, event.x, event.y)
        end
    end
end

def check_timer_button(button, event_x, event_y)
    # puts event_x
    # puts event_y
    if button.button_check(event_x, event_y)
        # puts "true"
        # puts button.button_num
        timer_press(button.button_type, button.button_num)
    else
        # puts "false"
    end
end

def timer_press(button_type, num)
    if button_type == "num"
        $my_timer.add_num(num)
        print $my_timer.timer_nums
        update_display
    elsif button_type == "clear"
        $my_timer.reset
        update_display
    elsif button_type == "set"
        # convert timer_nums into timer
        # start timer
    end
end

def update_display
    # first build string.  then insert into text.
    time = $my_timer[0].to_s + $my_timer[1].to_s + ':'
    time += $my_timer[2].to_s + $my_timer[3].to_s + ':'
    time += $my_timer[4].to_s + $my_timer[5].to_s
    # puts time
    $timer_text.remove
    $timer_text = Text.new(time, x:  221, y: 131, size: 60, color: 'black', rotate: 0, z: 10)
end

# get :width
# get :height
show