require 'ruby2d'

class Commands
    # def self.make_image(filename, x_val, y_val, width, height, rotation, z)
    #     Image.new (
    #         filename,
    #         x = 100, y: 100,
    #         width: width, height: height,
    #         rotate: rotation,
    #         z: z
    #     )
    # end

    def self.make_image(filename, x_val, y_val, w, h, rot, z_val)
        # puts "howdy"
        # set title: my_title
        Image.new(filename, x: x_val, y: y_val, width: w, height: h, rotate: 0, z: 10)
    end
end

class Dino
    def initialize(filename, pos_index)
        @dino_img = setup_dino(filename, pos_index)
    end

    def setup_dino(filename, pos_index)
        if pos_index == 1
            return Image.new(filename, x: 100, y: 150, width: 60, height: 60, rotate: 0, z: 10)
        elsif pos_index == 2
            return Image.new(filename, x: 100, y: 230, width: 60, height: 60, rotate: 0, z: 10)
        elsif pos_index == 3
            return Image.new(filename, x: 100, y: 310, width: 60, height: 60, rotate: 0, z: 10)
        else
            return Image.new(filename, x: 100, y: 390, width: 60, height: 60, rotate: 0, z: 10)
        end
    end
end

class Button
    attr_accessor :button_type, :button_num
    def initialize(x_left, x_right, y_top, y_bot, button_type, button_num)
        @x_left = x_left
        @x_right = x_right
        @y_top = y_top
        @y_bot = y_bot
        @button_type = button_type
        @button_num = button_num
    end

    def button_check(x_value, y_value)
        # puts "x_left: " + @x_left.to_s
        # puts "x_right: " + @x_right.to_s
        # puts "y_bot: " + @y_bot.to_s
        # puts "y_top: " + @y_top.to_s
        # puts "x: " + x_value.to_s
        # puts "y: " + y_value.to_s
        if x_value > @x_left && x_value < @x_right
            if y_value < @y_bot && y_value > @y_top
                return true
            end
        end

        return false
    end
end

class Timer
    attr_accessor :timer_nums
    def initialize
        @timer_nums = [0, 0, 0, 0, 0, 0]
    end

    def reset
        # puts "called reset"
        @timer_nums = [0, 0, 0, 0, 0, 0]
    end

    def [](val)
        return @timer_nums[val]
    end

    def add_num(num)
        @timer_nums << num
        @timer_nums.shift
    end
end

# class Snake
#     GRID_SIZE = 20
    
#     def initialize
#         @positions = [[2,0], [2,1], [2,2], [2,3]]
#     end

#     def draw
#         # debugger
#         @positions.each do |position|
#             Square.new(x:position[0] * GRID_SIZE, y: position[1] * GRID_SIZE, size: GRID_SIZE, color: 'white')
#         end
#     end
# end