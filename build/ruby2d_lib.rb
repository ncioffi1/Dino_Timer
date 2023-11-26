# String#ruby2d_colorize

# Extend `String` to include some fancy colors
class String
  def ruby2d_colorize(c); "\e[#{c}m#{self}\e[0m" end
  def bold;    ruby2d_colorize('1')    end
  def info;    ruby2d_colorize('1;34') end
  def warn;    ruby2d_colorize('1;33') end
  def success; ruby2d_colorize('1;32') end
  def error;   ruby2d_colorize('1;31') end
end


# frozen_string_literal: true

module Ruby2D
  # Ruby2D::Error
  class Error < StandardError
  end
end


# frozen_string_literal: true

# Ruby2D::Renderable

module Ruby2D
  # Base class for all renderable shapes
  module Renderable
    attr_reader :x, :y, :z, :width, :height, :color

    # Set the z position (depth) of the object
    def z=(z)
      remove
      @z = z
      add
    end

    # Add the object to the window
    def add
      Window.add(self)
    end

    # Remove the object from the window
    def remove
      Window.remove(self)
    end

    # Set the color value
    def color=(color)
      @color = Color.new(color)
    end

    # Allow British English spelling of color
    alias colour color
    alias colour= color=

    # Add a contains method stub
    def contains?(x, y)
      x >= @x && x <= (@x + @width) && y >= @y && y <= (@y + @height)
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Color

module Ruby2D
  # Colors are represented by the Color class. Colors can be created from keywords
  # (based on css), a hexadecimal value or an array containing a collection of
  # red, green, blue, and alpha (transparency) values expressed as a Float from 0.0 to 1.0.
  class Color
    # Color::Set represents an array of colors.
    class Set
      include Enumerable

      def initialize(colors)
        @colors = colors.map { |c| Color.new(c) }
      end

      def [](index)
        @colors[index]
      end

      def length
        @colors.length
      end

      alias count length

      def each(&block)
        @colors.each(&block)
      end

      def first
        @colors.first
      end

      def last
        @colors.last
      end

      def opacity
        @colors.first.opacity
      end

      def opacity=(opacity)
        @colors.each do |color|
          color.opacity = opacity
        end
      end
    end

    attr_accessor :r, :g, :b, :a

    # Based on clrs.cc
    NAMED_COLORS = {
      'navy' => '#001F3F',
      'blue' => '#0074D9',
      'aqua' => '#7FDBFF',
      'teal' => '#39CCCC',
      'olive' => '#3D9970',
      'green' => '#2ECC40',
      'lime' => '#01FF70',
      'yellow' => '#FFDC00',
      'orange' => '#FF851B',
      'red' => '#FF4136',
      'brown' => '#663300',
      'fuchsia' => '#F012BE',
      'purple' => '#B10DC9',
      'maroon' => '#85144B',
      'white' => '#FFFFFF',
      'silver' => '#DDDDDD',
      'gray' => '#AAAAAA',
      'black' => '#111111',
      'random' => ''
    }.freeze

    def initialize(color)
      raise Error, "`#{color}` is not a valid color" unless self.class.valid? color

      case color
      when String
        init_from_string color
      when Array
        @r = color[0]
        @g = color[1]
        @b = color[2]
        @a = color[3]
      when Color
        @r = color.r
        @g = color.g
        @b = color.b
        @a = color.a
      end
    end

    class << self
      # Return a color set if an array of valid colors
      def set(colors)
        # If a valid array of colors, return a `Color::Set` with those colors
        if colors.is_a?(Array) && colors.all? { |el| Color.valid? el }
          Color::Set.new(colors)
        # Otherwise, return single color
        else
          Color.new(colors)
        end
      end

      # Check if string is a proper hex value
      def hex?(color_string)
        # MRuby doesn't support regex, otherwise we'd do:
        #   !(/^#[0-9A-F]{6}$/i.match(a).nil?)
        color_string.instance_of?(String) &&
          color_string[0] == '#' &&
          color_string.length == 7
      end

      # Check if the color is valid
      def valid?(color)
        color.is_a?(Color) ||             # color object
          NAMED_COLORS.key?(color) ||     # keyword
          hex?(color) ||                  # hexadecimal value
          (                               # Array of Floats from 0.0..1.0
            color.instance_of?(Array) &&
            color.length == 4 &&
            color.all? { |el| el.is_a?(Numeric) }
          )
      end

      #
      # Backwards compatibility
      alias is_valid? valid?
      alias is_hex? valid?
    end

    # Convenience methods to alias `opacity` to `@a`
    def opacity
      @a
    end

    def opacity=(opacity)
      @a = opacity
    end

    # Return colour components as an array +[r, g, b, a]+
    def to_a
      [@r, @g, @b, @a]
    end

    private

    def init_from_string(color)
      if color == 'random'
        init_random_color
      elsif self.class.hex?(color)
        @r, @g, @b, @a = hex_to_f(color)
      else
        @r, @g, @b, @a = hex_to_f(NAMED_COLORS[color])
      end
    end

    def init_random_color
      @r = rand
      @g = rand
      @b = rand
      @a = 1.0
    end

    # Convert from Fixnum (0..255) to Float (0.0..1.0)
    def i_to_f(color_array)
      b = []
      color_array.each do |n|
        b.push(n / 255.0)
      end
      b
    end

    # Convert from hex value (e.g. #FFF000) to Float (0.0..1.0)
    def hex_to_f(hex_color)
      hex_color = (hex_color[1..]).chars.each_slice(2).map(&:join)
      a = []

      hex_color.each do |el|
        a.push(el.to_i(16))
      end

      a.push(255)
      i_to_f(a)
    end
  end

  # Allow British English spelling of color
  Colour = Color
end


# frozen_string_literal: true

# Ruby2D::Window

module Ruby2D
  # Represents a window on screen, responsible for storing renderable graphics,
  # event handlers, the update loop, showing and closing the window.

  attr_reader :width, :height

  class Window
    # Event structures
    EventDescriptor       = Struct.new(:type, :id)
    MouseEvent            = Struct.new(:type, :button, :direction, :x, :y, :delta_x, :delta_y)
    KeyEvent              = Struct.new(:type, :key)
    ControllerEvent       = Struct.new(:which, :type, :axis, :value, :button)
    ControllerAxisEvent   = Struct.new(:which, :axis, :value)
    ControllerButtonEvent = Struct.new(:which, :button)

    #
    # Create a Window
    # @param title [String] Title for the window
    # @param width [Numeric] In pixels
    # @param height [Numeric] in pixels
    # @param fps_cap [Numeric] Over-ride the default (60fps) frames-per-second
    # @param vsync [Boolean] Enabled by default, use this to override it (Not recommended)
    def initialize(title: 'Ruby 2D', width: 640, height: 480, fps_cap: 60, vsync: true)
      # Title of the window
      @title = title

      # Window size
      @width  = width
      @height = height

      # Frames per second upper limit, and the actual FPS
      @fps_cap = fps_cap
      @fps = @fps_cap

      # Vertical synchronization, set to prevent screen tearing (recommended)
      @vsync = vsync

      # Total number of frames that have been rendered
      @frames = 0

      # Renderable objects currently in the window, like a linear scene graph
      @objects = []

      _init_window_defaults
      _init_event_stores
      _init_event_registrations
      _init_procs_dsl_console
    end

    # Track open window state in a class instance variable
    @open_window = false

    # Class methods for convenient access to properties
    class << self
      def current
        get(:window)
      end

      def title
        get(:title)
      end

      def background
        get(:background)
      end

      def width
        get(:width)
      end

      def height
        get(:height)
      end

      def viewport_width
        get(:viewport_width)
      end

      def viewport_height
        get(:viewport_height)
      end

      def display_width
        get(:display_width)
      end

      def display_height
        get(:display_height)
      end

      def resizable
        get(:resizable)
      end

      def borderless
        get(:borderless)
      end

      def fullscreen
        get(:fullscreen)
      end

      def highdpi
        get(:highdpi)
      end

      def frames
        get(:frames)
      end

      def fps
        get(:fps)
      end

      def fps_cap
        get(:fps_cap)
      end

      def mouse_x
        get(:mouse_x)
      end

      def mouse_y
        get(:mouse_y)
      end

      def diagnostics
        get(:diagnostics)
      end

      def screenshot(opts = nil)
        get(:screenshot, opts)
      end

      def get(sym, opts = nil)
        DSL.window.get(sym, opts)
      end

      def set(opts)
        DSL.window.set(opts)
      end

      def on(event, &proc)
        DSL.window.on(event, &proc)
      end

      def off(event_descriptor)
        DSL.window.off(event_descriptor)
      end

      def add(object)
        DSL.window.add(object)
      end

      def remove(object)
        DSL.window.remove(object)
      end

      def clear
        DSL.window.clear
      end

      def update(&proc)
        DSL.window.update(&proc)
      end

      def render(&proc)
        DSL.window.render(&proc)
      end

      def show
        DSL.window.show
      end

      def close
        DSL.window.close
      end

      def render_ready_check
        return if opened?

        raise Error,
              'Attempting to draw before the window is ready. Please put calls to draw() inside of a render block.'
      end

      def opened?
        @open_window
      end

      private

      def opened!
        @open_window = true
      end
    end

    # Public instance methods

    # --- start exception
    # Exception from lint check for the #get method which is what it is. :)
    #
    # rubocop:disable Metrics/CyclomaticComplexity
    # rubocop:disable Metrics/AbcSize

    # Retrieve an attribute of the window
    # @param sym [Symbol] The name of an attribute to retrieve.
    def get(sym, opts = nil)
      case sym
      when :window then          self
      when :title then           @title
      when :background then      @background
      when :width then           @width
      when :height then          @height
      when :viewport_width then  @viewport_width
      when :viewport_height then @viewport_height
      when :display_width, :display_height
        ext_get_display_dimensions
        if sym == :display_width
          @display_width
        else
          @display_height
        end
      when :resizable then       @resizable
      when :borderless then      @borderless
      when :fullscreen then      @fullscreen
      when :highdpi then         @highdpi
      when :frames then          @frames
      when :fps then             @fps
      when :fps_cap then         @fps_cap
      when :mouse_x then         @mouse_x
      when :mouse_y then         @mouse_y
      when :diagnostics then     @diagnostics
      when :screenshot then      screenshot(opts)
      end
    end
    # rubocop:enable Metrics/CyclomaticComplexity
    # rubocop:enable Metrics/AbcSize
    # --- end exception

    # Set a window attribute
    # @param opts [Hash] The attributes to set
    # @option opts [Color] :background
    # @option opts [String] :title
    # @option opts [Numeric] :width
    # @option opts [Numeric] :height
    # @option opts [Numeric] :viewport_width
    # @option opts [Numeric] :viewport_height
    # @option opts [Boolean] :highdpi
    # @option opts [Boolean] :resizable
    # @option opts [Boolean] :borderless
    # @option opts [Boolean] :fullscreen
    # @option opts [Numeric] :fps_cap
    # @option opts [Numeric] :diagnostics
    def set(opts)
      # Store new window attributes, or ignore if nil
      _set_any_window_properties opts
      _set_any_window_dimensions opts

      @fps_cap = opts[:fps_cap] if opts[:fps_cap]
      return if opts[:diagnostics].nil?

      @diagnostics = opts[:diagnostics]
      ext_diagnostics(@diagnostics)
    end

    # Add an object to the window
    def add(object)
      case object
      when nil
        raise Error, "Cannot add '#{object.class}' to window!"
      when Array
        object.each { |x| add_object(x) }
      else
        add_object(object)
      end
    end

    # Remove an object from the window
    def remove(object)
      raise Error, "Cannot remove '#{object.class}' from window!" if object.nil?

      ix = @objects.index(object)
      return false if ix.nil?

      @objects.delete_at(ix)
      true
    end

    # Clear all objects from the window
    def clear
      @objects.clear
    end

    # Set the update callback
    def update(&proc)
      @update_proc = proc
      true
    end

    # Set the render callback
    def render(&proc)
      @render_proc = proc
      true
    end

    # Generate a new event key (ID)
    def new_event_key
      @event_key = @event_key.next
    end

    # Set an event handler
    def on(event, &proc)
      raise Error, "`#{event}` is not a valid event type" unless @events.key? event

      event_id = new_event_key
      @events[event][event_id] = proc
      EventDescriptor.new(event, event_id)
    end

    # Remove an event handler
    def off(event_descriptor)
      @events[event_descriptor.type].delete(event_descriptor.id)
    end

    # Key down event method for class pattern
    def key_down(key)
      @keys_down.include? key
    end

    # Key held event method for class pattern
    def key_held(key)
      @keys_held.include? key
    end

    # Key up event method for class pattern
    def key_up(key)
      @keys_up.include? key
    end

    # Key callback method, called by the native and web extentions
    def key_callback(type, key)
      key = key.downcase

      # All key events
      @events[:key].each do |_id, e|
        e.call(KeyEvent.new(type, key))
      end

      case type
      # When key is pressed, fired once
      when :down
        _handle_key_down type, key
      # When key is being held down, fired every frame
      when :held
        _handle_key_held type, key
      # When key released, fired once
      when :up
        _handle_key_up type, key
      end
    end

    # Mouse down event method for class pattern
    def mouse_down(btn)
      @mouse_buttons_down.include? btn
    end

    # Mouse up event method for class pattern
    def mouse_up(btn)
      @mouse_buttons_up.include? btn
    end

    # Mouse scroll event method for class pattern
    def mouse_scroll
      @mouse_scroll_event
    end

    # Mouse move event method for class pattern
    def mouse_move
      @mouse_move_event
    end

    # Mouse callback method, called by the native and web extentions
    def mouse_callback(type, button, direction, x, y, delta_x, delta_y)
      # All mouse events
      @events[:mouse].each do |_id, e|
        e.call(MouseEvent.new(type, button, direction, x, y, delta_x, delta_y))
      end

      case type
      # When mouse button pressed
      when :down
        _handle_mouse_down type, button, x, y
      # When mouse button released
      when :up
        _handle_mouse_up type, button, x, y
      # When mouse motion / movement
      when :scroll
        _handle_mouse_scroll type, direction, delta_x, delta_y
      # When mouse scrolling, wheel or trackpad
      when :move
        _handle_mouse_move type, x, y, delta_x, delta_y
      end
    end

    # Add controller mappings from file
    def add_controller_mappings
      ext_add_controller_mappings(@controller_mappings) if File.exist? @controller_mappings
    end

    # Controller axis event method for class pattern
    def controller_axis(axis)
      @controller_axes_moved.include? axis
    end

    # Controller button down event method for class pattern
    def controller_button_down(btn)
      @controller_buttons_down.include? btn
    end

    # Controller button up event method for class pattern
    def controller_button_up(btn)
      @controller_buttons_up.include? btn
    end

    # Controller callback method, called by the native and web extentions
    def controller_callback(which, type, axis, value, button)
      # All controller events
      @events[:controller].each do |_id, e|
        e.call(ControllerEvent.new(which, type, axis, value, button))
      end

      case type
      # When controller axis motion, like analog sticks
      when :axis
        _handle_controller_axis which, axis, value
      # When controller button is pressed
      when :button_down
        _handle_controller_button_down which, button
      # When controller button is released
      when :button_up
        _handle_controller_button_up which, button
      end
    end

    # Update callback method, called by the native and web extentions
    def update_callback
      update unless @using_dsl

      @update_proc.call

      # Accept and eval commands if in console mode
      _handle_console_input if @console && $stdin.ready?

      # Clear inputs if using class pattern
      _clear_event_stores unless @using_dsl
    end

    # Render callback method, called by the native and web extentions
    def render_callback
      render unless @using_dsl

      @render_proc.call
    end

    # Show the window
    def show
      raise Error, 'Window#show called multiple times, Ruby2D only supports a single open window' if Window.opened?

      Window.send(:opened!)
      ext_show
    end

    # Take screenshot
    def screenshot(path)
      if path
        ext_screenshot(path)
      else
        time = if RUBY_ENGINE == 'ruby'
                 Time.now.utc.strftime '%Y-%m-%d--%H-%M-%S'
               else
                 Time.now.utc.to_i
               end
        ext_screenshot("./screenshot-#{time}.png")
      end
    end

    # Close the window
    def close
      ext_close
    end

    # Private instance methods

    private

    # An an object to the window, used by the public `add` method
    def add_object(object)
      if !@objects.include?(object)
        index = @objects.index do |obj|
          obj.z > object.z
        end
        if index
          @objects.insert(index, object)
        else
          @objects.push(object)
        end
        true
      else
        false
      end
    end

    def _set_any_window_properties(opts)
      @background = Color.new(opts[:background]) if Color.valid? opts[:background]
      @title           = opts[:title]           if opts[:title]
      @icon            = opts[:icon]            if opts[:icon]
      @resizable       = opts[:resizable]       if opts[:resizable]
      @borderless      = opts[:borderless]      if opts[:borderless]
      @fullscreen      = opts[:fullscreen]      if opts[:fullscreen]
    end

    def _set_any_window_dimensions(opts)
      @width           = opts[:width]           if opts[:width]
      @height          = opts[:height]          if opts[:height]
      @viewport_width  = opts[:viewport_width]  if opts[:viewport_width]
      @viewport_height = opts[:viewport_height] if opts[:viewport_height]
      @highdpi         = opts[:highdpi] unless opts[:highdpi].nil?
    end

    def _handle_key_down(type, key)
      # For class pattern
      @keys_down << key if !@using_dsl && !(@keys_down.include? key)

      # Call event handler
      @events[:key_down].each do |_id, e|
        e.call(KeyEvent.new(type, key))
      end
    end

    def _handle_key_held(type, key)
      # For class pattern
      @keys_held << key if !@using_dsl && !(@keys_held.include? key)

      # Call event handler
      @events[:key_held].each do |_id, e|
        e.call(KeyEvent.new(type, key))
      end
    end

    def _handle_key_up(type, key)
      # For class pattern
      @keys_up << key if !@using_dsl && !(@keys_up.include? key)

      # Call event handler
      @events[:key_up].each do |_id, e|
        e.call(KeyEvent.new(type, key))
      end
    end

    def _handle_mouse_down(type, button, x, y)
      # For class pattern
      @mouse_buttons_down << button if !@using_dsl && !(@mouse_buttons_down.include? button)

      # Call event handler
      @events[:mouse_down].each do |_id, e|
        e.call(MouseEvent.new(type, button, nil, x, y, nil, nil))
      end
    end

    def _handle_mouse_up(type, button, x, y)
      # For class pattern
      @mouse_buttons_up << button if !@using_dsl && !(@mouse_buttons_up.include? button)

      # Call event handler
      @events[:mouse_up].each do |_id, e|
        e.call(MouseEvent.new(type, button, nil, x, y, nil, nil))
      end
    end

    def _handle_mouse_scroll(type, direction, delta_x, delta_y)
      # For class pattern
      unless @using_dsl
        @mouse_scroll_event     = true
        @mouse_scroll_direction = direction
        @mouse_scroll_delta_x   = delta_x
        @mouse_scroll_delta_y   = delta_y
      end

      # Call event handler
      @events[:mouse_scroll].each do |_id, e|
        e.call(MouseEvent.new(type, nil, direction, nil, nil, delta_x, delta_y))
      end
    end

    def _handle_mouse_move(type, x, y, delta_x, delta_y)
      # For class pattern
      unless @using_dsl
        @mouse_move_event   = true
        @mouse_move_delta_x = delta_x
        @mouse_move_delta_y = delta_y
      end

      # Call event handler
      @events[:mouse_move].each do |_id, e|
        e.call(MouseEvent.new(type, nil, nil, x, y, delta_x, delta_y))
      end
    end

    def _handle_controller_axis(which, axis, value)
      # For class pattern
      unless @using_dsl
        @controller_id = which
        @controller_axes_moved << axis unless @controller_axes_moved.include? axis
        _set_controller_axis_value axis, value
      end

      # Call event handler
      @events[:controller_axis].each do |_id, e|
        e.call(ControllerAxisEvent.new(which, axis, value))
      end
    end

    def _set_controller_axis_value(axis, value)
      case axis
      when :left_x
        @controller_axis_left_x = value
      when :left_y
        @controller_axis_left_y = value
      when :right_x
        @controller_axis_right_x = value
      when :right_y
        @controller_axis_right_y = value
      end
    end

    def _handle_controller_button_down(which, button)
      # For class pattern
      unless @using_dsl
        @controller_id = which
        @controller_buttons_down << button unless @controller_buttons_down.include? button
      end

      # Call event handler
      @events[:controller_button_down].each do |_id, e|
        e.call(ControllerButtonEvent.new(which, button))
      end
    end

    def _handle_controller_button_up(which, button)
      # For class pattern
      unless @using_dsl
        @controller_id = which
        @controller_buttons_up << button unless @controller_buttons_up.include? button
      end

      # Call event handler
      @events[:controller_button_up].each do |_id, e|
        e.call(ControllerButtonEvent.new(which, button))
      end
    end

    # --- start exception
    # Exception from lint check for this method only
    #
    # rubocop:disable Lint/RescueException
    # rubocop:disable Security/Eval
    def _handle_console_input
      cmd = $stdin.gets
      begin
        res = eval(cmd, TOPLEVEL_BINDING)
        $stdout.puts "=> #{res.inspect}"
        $stdout.flush
      rescue SyntaxError => e
        $stdout.puts e
        $stdout.flush
      rescue Exception => e
        $stdout.puts e
        $stdout.flush
      end
    end
    # rubocop:enable Lint/RescueException
    # rubocop:enable Security/Eval
    # ---- end exception

    def _clear_event_stores
      @keys_down.clear
      @keys_held.clear
      @keys_up.clear
      @mouse_buttons_down.clear
      @mouse_buttons_up.clear
      @mouse_scroll_event = false
      @mouse_move_event = false
      @controller_axes_moved.clear
      @controller_buttons_down.clear
      @controller_buttons_up.clear
    end

    def _init_window_defaults
      # Window background color
      @background = Color.new([0.0, 0.0, 0.0, 1.0])

      # Window icon
      @icon = nil

      # Window characteristics
      @resizable = false
      @borderless = false
      @fullscreen = false
      @highdpi = false

      # Size of the window's viewport (the drawable area)
      @viewport_width = nil
      @viewport_height = nil

      # Size of the computer's display
      @display_width = nil
      @display_height = nil
    end

    def _init_event_stores
      _init_key_event_stores
      _init_mouse_event_stores
      _init_controller_event_stores
    end

    def _init_key_event_stores
      # Event stores for class pattern
      @keys_down = []
      @keys_held = []
      @keys_up   = []
    end

    def _init_mouse_event_stores
      @mouse_buttons_down = []
      @mouse_buttons_up   = []
      @mouse_scroll_event     = false
      @mouse_scroll_direction = nil
      @mouse_scroll_delta_x   = 0
      @mouse_scroll_delta_y   = 0
      @mouse_move_event   = false
      @mouse_move_delta_x = 0
      @mouse_move_delta_y = 0
    end

    def _init_controller_event_stores
      @controller_id = nil
      @controller_axes_moved   = []
      @controller_axis_left_x  = 0
      @controller_axis_left_y  = 0
      @controller_axis_right_x = 0
      @controller_axis_right_y = 0
      @controller_buttons_down = []
      @controller_buttons_up   = []
    end

    def _init_event_registrations
      # Mouse X and Y position in the window
      @mouse_x = 0
      @mouse_y = 0

      # Controller axis and button mappings file
      @controller_mappings = "#{File.expand_path('~')}/.ruby2d/controllers.txt"

      # Unique ID for the input event being registered
      @event_key = 0

      # Registered input events
      @events = {
        key: {},
        key_down: {},
        key_held: {},
        key_up: {},
        mouse: {},
        mouse_up: {},
        mouse_down: {},
        mouse_scroll: {},
        mouse_move: {},
        controller: {},
        controller_axis: {},
        controller_button_down: {},
        controller_button_up: {}
      }
    end

    def _init_procs_dsl_console
      # The window update block
      @update_proc = proc {}

      # The window render block
      @render_proc = proc {}

      # Detect if window is being used through the DSL or as a class instance
      @using_dsl = !(method(:update).parameters.empty? || method(:render).parameters.empty?)

      # Whether diagnostic messages should be printed
      @diagnostics = false

      # Console mode, enabled at command line
      @console = if RUBY_ENGINE == 'ruby'
                   ENV['RUBY2D_ENABLE_CONSOLE'] == 'true'
                 else
                   false
                 end
    end
  end
end


# frozen_string_literal: true

module Ruby2D
  # Ruby2D::DSL
  module DSL
    @window = Ruby2D::Window.new

    def self.window
      @window
    end

    def self.window=(window)
      @window = window
    end

    def get(sym, opts = nil)
      DSL.window.get(sym, opts)
    end

    def set(opts)
      DSL.window.set(opts)
    end

    def on(event, &proc)
      DSL.window.on(event, &proc)
    end

    def off(event_descriptor)
      DSL.window.off(event_descriptor)
    end

    def update(&proc)
      DSL.window.update(&proc)
    end

    def render(&proc)
      DSL.window.render(&proc)
    end

    def clear
      DSL.window.clear
    end

    def show
      DSL.window.show
    end

    def close
      DSL.window.close
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Quad

module Ruby2D
  # A quadrilateral based on four points in clockwise order starting at the top left.
  class Quad
    include Renderable

    # Coordinates in clockwise order, starting at top left:
    # x1,y1 == top left
    # x2,y2 == top right
    # x3,y3 == bottom right
    # x4,y4 == bottom left
    attr_accessor :x1, :y1,
                  :x2, :y2,
                  :x3, :y3,
                  :x4, :y4

    # Create an quadrilateral
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Numeric] x4
    # @param [Numeric] y4
    # @param [Numeric] z
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @param [Numeric] opacity Opacity of the image when rendering
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def initialize(x1: 0, y1: 0, x2: 100, y2: 0, x3: 100, y3: 100, x4: 0, y4: 100,
                   z: 0, color: nil, colour: nil, opacity: nil)
      @x1 = x1
      @y1 = y1
      @x2 = x2
      @y2 = y2
      @x3 = x3
      @y3 = y3
      @x4 = x4
      @y4 = y4
      @z  = z
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?
      add
    end

    # Change the colour of the line
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def color=(color)
      # convert to Color or Color::Set
      color = Color.set(color)

      # require 4 colours if multiple colours provided
      if color.is_a?(Color::Set) && color.length != 4
        raise ArgumentError,
              "`#{self.class}` requires 4 colors, one for each vertex. #{color.length} were given."
      end

      @color = color # converted above
      invalidate_color_components
    end

    # The logic is the same as for a triangle
    # See triangle.rb for reference
    def contains?(x, y)
      self_area = triangle_area(@x1, @y1, @x2, @y2, @x3, @y3) +
                  triangle_area(@x1, @y1, @x3, @y3, @x4, @y4)

      questioned_area = triangle_area(@x1, @y1, @x2, @y2, x, y) +
                        triangle_area(@x2, @y2, @x3, @y3, x, y) +
                        triangle_area(@x3, @y3, @x4, @y4, x, y) +
                        triangle_area(@x4, @y4, @x1, @y1, x, y)

      questioned_area <= self_area
    end

    # Draw a line without creating a Line
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Numeric] x4
    # @param [Numeric] y4
    # @param color [Array<Array<float,float,float,float>>] An array of 4 array of colour components
    #                                       (e.g. [[1.0, 0, 0, 1.0], ...])
    def self.draw(x1:, y1:, x2:, y2:, x3:, y3:, x4:, y4:, color:)
      Window.render_ready_check
      ext_draw([
                 x1, y1, *color[0], # splat the colour components
                 x2, y2, *color[1],
                 x3, y3, *color[2],
                 x4, y4, *color[3]
               ])
    end

    private

    def render
      color_comp_arrays = color_components
      self.class.ext_draw([
                            @x1, @y1, *color_comp_arrays[0], # splat the colour components
                            @x2, @y2, *color_comp_arrays[1],
                            @x3, @y3, *color_comp_arrays[2],
                            @x4, @y4, *color_comp_arrays[3]
                          ])
    end

    def triangle_area(x1, y1, x2, y2, x3, y3)
      (x1 * y2 + x2 * y3 + x3 * y1 - x3 * y2 - x1 * y3 - x2 * y1).abs / 2
    end

    # Return colours as a memoized array of 4 x colour component arrays
    def color_components
      check_if_opacity_changed
      @color_components ||= if @color.is_a? Color::Set
                              # Extract colour component arrays; see +def color=+ where colour set
                              # size is enforced
                              [
                                @color[0].to_a, @color[1].to_a, @color[2].to_a, @color[3].to_a
                              ]
                            else
                              # All vertex colours are the same
                              c_a = @color.to_a
                              [
                                c_a, c_a, c_a, c_a
                              ]
                            end
    end

    # Invalidate memoized colour components if opacity has been changed via +color=+
    def check_if_opacity_changed
      @color_components = nil if @color_components && @color_components.first[3] != @color.opacity
    end

    # Invalidate the memoized colour components. Called when Line's colour is changed
    def invalidate_color_components
      @color_components = nil
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Line

module Ruby2D
  # A line between two points.
  class Line
    include Renderable

    attr_accessor :x1, :x2, :y1, :y2, :width

    # Create an Line
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] width The +width+ or thickness of the line
    # @param [Numeric] z
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @param [Numeric] opacity Opacity of the image when rendering
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def initialize(x1: 0, y1: 0, x2: 100, y2: 100, z: 0,
                   width: 2, color: nil, colour: nil, opacity: nil)
      @x1 = x1
      @y1 = y1
      @x2 = x2
      @y2 = y2
      @z = z
      @width = width
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?
      add
    end

    # Change the colour of the line
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def color=(color)
      # convert to Color or Color::Set
      color = Color.set(color)

      # require 4 colours if multiple colours provided
      if color.is_a?(Color::Set) && color.length != 4
        raise ArgumentError,
              "`#{self.class}` requires 4 colors, one for each vertex. #{color.length} were given."
      end

      @color = color # converted above
      invalidate_color_components
    end

    # Return the length of the line
    def length
      points_distance(@x1, @y1, @x2, @y2)
    end

    # Line contains a point if the point is closer than the length of line from
    # both ends and if the distance from point to line is smaller than half of
    # the width. For reference:
    #   https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
    def contains?(x, y)
      line_len = length
      points_distance(@x1, @y1, x, y) <= line_len &&
        points_distance(@x2, @y2, x, y) <= line_len &&
        (((@y2 - @y1) * x - (@x2 - @x1) * y + @x2 * @y1 - @y2 * @x1).abs / line_len) <= 0.5 * @width
    end

    # Draw a line without creating a Line
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] width The +width+ or thickness of the line
    # @param [Array] colors An array or 4 arrays of colour components.
    def self.draw(x1:, y1:, x2:, y2:, width:, color:)
      Window.render_ready_check

      ext_draw([
                 x1, y1, x2, y2, width,
                 # splat each colour's components
                 *color[0], *color[1], *color[2], *color[3]
               ])
    end

    private

    def render
      self.class.ext_draw([
                            @x1, @y1, @x2, @y2, @width,
                            # splat the colour components from helper
                            *color_components
                          ])
    end

    # Calculate the distance between two points
    def points_distance(x1, y1, x2, y2)
      Math.sqrt((x1 - x2).abs2 + (y1 - y2).abs2)
    end

    # Return colours as a memoized flattened array of 4 x colour components
    def color_components
      check_if_opacity_changed
      @color_components ||= if @color.is_a? Color::Set
                              # Splat the colours from the set; see +def color=+ where colour set
                              # size is enforced
                              [
                                *@color[0].to_a, *@color[1].to_a, *@color[2].to_a, *@color[3].to_a
                              ]
                            else
                              # All vertex colours are the same
                              c_a = @color.to_a
                              [
                                *c_a, *c_a, *c_a, *c_a
                              ]
                            end
    end

    # Invalidate memoized colour components if opacity has been changed via +color=+
    def check_if_opacity_changed
      @color_components = nil if @color_components && @color_components[3] != @color.opacity
    end

    # Invalidate the memoized colour components. Called when Line's colour is changed
    def invalidate_color_components
      @color_components = nil
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Circle

module Ruby2D
  #
  # Create a circle using +Circle.new+
  #
  class Circle
    include Renderable

    attr_accessor :x, :y, :radius, :sectors

    # Create a circle
    #
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] z
    # @param [Numeric] radius
    # @param [Numeric] sectors Smoothness of the circle is better when more +sectors+ are used.
    # @param [String | Color] color Or +colour+
    # @param [Float] opacity
    def initialize(x: 25, y: 25, z: 0, radius: 50, sectors: 30,
                   color: nil, colour: nil, opacity: nil)
      @x = x
      @y = y
      @z = z
      @radius = radius
      @sectors = sectors
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?
      add
    end

    # Check if the circle contains the point at +(x, y)+
    def contains?(x, y)
      Math.sqrt((x - @x)**2 + (y - @y)**2) <= @radius
    end

    def self.draw(opts = {})
      Window.render_ready_check

      ext_draw([
                 opts[:x], opts[:y], opts[:radius], opts[:sectors],
                 opts[:color][0], opts[:color][1], opts[:color][2], opts[:color][3]
               ])
    end

    private

    def render
      self.class.ext_draw([
                            @x, @y, @radius, @sectors,
                            @color.r, @color.g, @color.b, @color.a
                          ])
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Rectangle

module Ruby2D
  # A rectangle
  class Rectangle < Quad
    # Create an rectangle
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] width
    # @param [Numeric] height
    # @param [Numeric] z
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @param [Numeric] opacity Opacity of the image when rendering
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def initialize(x: 0, y: 0, width: 200, height: 100, z: 0, color: nil, colour: nil, opacity: nil)
      @width = width
      @height = height
      super(x1: @x = x, y1: @y = y,
            x2: x + width, y2: y,
            x3: x + width, y3: y + height,
            x4: x, y4: y + height, z: z, color: color, colour: colour, opacity: opacity)
    end

    def x=(x)
      @x = @x1 = x
      @x2 = x + @width
      @x3 = x + @width
      @x4 = x
    end

    def y=(y)
      @y = @y1 = y
      @y2 = y
      @y3 = y + @height
      @y4 = y + @height
    end

    def width=(width)
      @width = width
      @x2 = @x1 + width
      @x3 = @x1 + width
    end

    def height=(height)
      @height = height
      @y3 = @y1 + height
      @y4 = @y1 + height
    end

    def self.draw(x:, y:, width:, height:, color:)
      super(x1: x, y1: y,
            x2: x + width, y2: y,
            x3: x + width, y3: y + height,
            x4: x, y4: y + height, color: color)
    end

    def contains?(x, y)
      # Override the check from Quad since this is faster
      x >= @x && x <= (@x + @width) && y >= @y && y <= (@y + @height)
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Square

module Ruby2D
  # A square
  class Square < Rectangle
    attr_reader :size

    # Create an square
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] size is width and height
    # @param [Numeric] z
    # @param [String, Array] color A single colour or an array of exactly 4 colours
    # @param [Numeric] opacity Opacity of the image when rendering
    # @raise [ArgumentError] if an array of colours does not have 4 entries
    def initialize(x: 0, y: 0, size: 100, z: 0, color: nil, colour: nil, opacity: nil)
      @size = size
      super(x: x, y: y, width: size, height: size, z: z,
            color: color, colour: colour, opacity: opacity)
    end

    # Set the size of the square
    def size=(size)
      self.width = self.height = @size = size
    end

    def self.draw(x:, y:, size:, color:)
      super(x: x, y: y,
            width: size, height: size,
            color: color)
    end

    # Make the inherited width and height attribute accessors private
    private :width=, :height=
  end
end


# frozen_string_literal: true

# Ruby2D::Triangle

module Ruby2D
  # A triangle
  class Triangle
    include Renderable

    attr_accessor :x1, :y1,
                  :x2, :y2,
                  :x3, :y3
    attr_reader :color

    # Create a triangle
    # @param x1 [Numeric]
    # @param y1 [Numeric]
    # @param x2 [Numeric]
    # @param y2 [Numeric]
    # @param x3 [Numeric]
    # @param y3 [Numeric]
    # @param z [Numeric]
    # @param color [String, Array] A single colour or an array of exactly 3 colours
    # @param opacity [Numeric] Opacity of the image when rendering
    # @raise [ArgumentError] if an array of colours does not have 3 entries
    def initialize(x1: 50, y1: 0, x2: 100, y2: 100, x3: 0, y3: 100,
                   z: 0, color: 'white', colour: nil, opacity: nil)
      @x1 = x1
      @y1 = y1
      @x2 = x2
      @y2 = y2
      @x3 = x3
      @y3 = y3
      @z  = z
      self.color = color || colour
      self.color.opacity = opacity if opacity
      add
    end

    # Change the colour of the line
    # @param [String, Array] color A single colour or an array of exactly 3 colours
    # @raise [ArgumentError] if an array of colours does not have 3 entries
    def color=(color)
      # convert to Color or Color::Set
      color = Color.set(color)

      # require 3 colours if multiple colours provided
      if color.is_a?(Color::Set) && color.length != 3
        raise ArgumentError,
              "`#{self.class}` requires 3 colors, one for each vertex. #{color.length} were given."
      end

      @color = color # converted above
      invalidate_color_components
    end

    # A point is inside a triangle if the area of 3 triangles, constructed from
    # triangle sides and the given point, is equal to the area of triangle.
    def contains?(x, y)
      self_area = triangle_area(@x1, @y1, @x2, @y2, @x3, @y3)
      questioned_area =
        triangle_area(@x1, @y1, @x2, @y2, x, y) +
        triangle_area(@x2, @y2, @x3, @y3, x, y) +
        triangle_area(@x3, @y3, @x1, @y1, x, y)

      questioned_area <= self_area
    end

    # Draw a triangle
    # @param x1 [Numeric]
    # @param y1 [Numeric]
    # @param x2 [Numeric]
    # @param y2 [Numeric]
    # @param x3 [Numeric]
    # @param y3 [Numeric]
    # @param z [Numeric]
    # @param color [Array<Array<float,float,float,float>>] An array of 3 arrays of colour components
    #                                  (e.g. [[1.0, 0, 0, 1.0], ...])
    def self.draw(x1:, y1:, x2:, y2:, x3:, y3:, color:)
      Window.render_ready_check
      ext_draw([
                 x1, y1, *color[0], # splat the colour components
                 x2, y2, *color[1],
                 x3, y3, *color[2]
               ])
    end

    private

    def render
      color_comp_arrays = color_components
      self.class.ext_draw([
                            @x1, @y1, *color_comp_arrays[0], # splat the colour components
                            @x2, @y2, *color_comp_arrays[1],
                            @x3, @y3, *color_comp_arrays[2]
                          ])
    end

    def triangle_area(x1, y1, x2, y2, x3, y3)
      (x1 * y2 + x2 * y3 + x3 * y1 - x3 * y2 - x1 * y3 - x2 * y1).abs / 2
    end

    # Return colours as a memoized array of 3 x colour component arrays
    def color_components
      check_if_opacity_changed
      @color_components ||= if @color.is_a? Color::Set
                              # Extract colour component arrays; see +def color=+ where colour set
                              # size is enforced
                              [
                                @color[0].to_a, @color[1].to_a, @color[2].to_a
                              ]
                            else
                              # All vertex colours are the same
                              c_a = @color.to_a
                              [
                                c_a, c_a, c_a
                              ]
                            end
    end

    # Invalidate memoized colour components if opacity has been changed via +color=+
    def check_if_opacity_changed
      @color_components = nil if @color_components && @color_components.first[3] != @color.opacity
    end

    # Invalidate the memoized colour components. Called when Line's colour is changed
    def invalidate_color_components
      @color_components = nil
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Pixel

module Ruby2D
  # Draw a single pixel by calling +Pixel.draw(...)+
  class Pixel
    def self.draw(x:, y:, size:, color:)
      color = color.to_a if color.is_a? Color

      ext_draw([x, y,
                x + size, y,
                x + size, y + size,
                x, y + size,
                color[0], color[1], color[2], color[3]])
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Pixmap

module Ruby2D
  # Error when failed to load an image
  class InvalidImageFileError < Ruby2D::Error
    def initialize(file_path)
      super("Failed to load image file: #{file_path}")
    end
  end

  # Error finding image file
  class UnknownImageFileError < Ruby2D::Error
    def initialize(file_path)
      super "Cannot find image file `#{file_path}`"
    end
  end

  #
  # A pixmap represents an image made up of pixel data of fixed width and height.
  class Pixmap
    attr_reader :width, :height, :path

    def initialize(file_path)
      file_path = file_path.to_s
      raise UnknownImageFileError, file_path unless File.exist? file_path

      ext_load_pixmap(file_path)
      raise InvalidImageFileError, file_path unless @ext_pixel_data

      @path = file_path
    end

    def texture
      @texture ||= Texture.new(@ext_pixel_data, @width, @height)
    end
  end
end


# frozen_string_literal: true

# Ruby2D::PixmapAtlas

module Ruby2D
  #
  # A pixmap is a 2D grid of pixel data of fixed width and height. A pixmap atlas
  # is a dictionary of named pixmaps where each named pixmap is loaded once and can be re-used
  # multiple times without the overhead of repeated loading and multiple copies.
  class PixmapAtlas
    def initialize
      @atlas = {} # empty map
    end

    # Empty the atlas, eventually releasing all the loaded
    # pixmaps (except for any that are referenced elsewhere)
    def clear
      @atlas.clear
    end

    # Find a previosly loaded Pixmap in the atlas by name
    #
    # @param [String] name
    # @return [Pixmap, nil]
    def [](name)
      @atlas[name]
    end

    # Get the number of Pixmaps in the atlas
    def count
      @atlas.count
    end

    # Iterate through each Pixmap in the atlas
    def each(&block)
      @atlas.each(&block)
    end

    # Load a pixmap from an image +file_path+ and store it +as+ named for later lookup.
    #
    # @param [String] file_path The path to the image file to load as a +Pixmap+
    # @param [String] as The name to associated with the Pixmap; if +nil+ then +file_path+ is used +as+ the name
    #
    # @raise [Pixmap::InvalidImageFileError] if the image file cannot be loaded
    # @return [Pixmap] the new (or existing) Pixmap
    def load_and_keep_image(file_path, as: nil)
      name = as || file_path
      if @atlas.member? name
        @atlas[name]
      else
        pixmap = Pixmap.new file_path
        @atlas[name] = pixmap
      end
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Image

module Ruby2D
  # Images in many popular formats can be drawn in the window.
  # To draw an image in the window, use the following, providing the image file path:
  # +Image.new('star.png')+
  class Image
    include Renderable

    attr_reader :path
    attr_accessor :x, :y, :width, :height, :rotate, :data

    # Load an image +path+ and return a Texture, using a pixmap atlas if provided
    # @param path [#to_s] The location of the file to load as an image.
    # @param atlas [PixmapAtlas] Pixmap atlas to use to manage the image file
    # @return [Texture] loaded
    def self.load_image_as_texture(path, atlas:)
      pixmap = if atlas
                 atlas.load_and_keep_image(path, as: path)
               else
                 Pixmap.new path
               end
      pixmap.texture
    end

    # Create an Image
    # @param path [#to_s] The location of the file to load as an image.
    # @param width [Numeric] The +width+ of image, or default is width from image file
    # @param height [Numeric] The +height+ of image, or default is height from image file
    # @param x [Numeric]
    # @param y [Numeric]
    # @param z [Numeric]
    # @param rotate [Numeric] Angle, default is 0
    # @param color [Numeric] (or +colour+) Tint the image when rendering
    # @param opacity [Numeric] Opacity of the image when rendering
    # @param show [Boolean] If +true+ the image is added to +Window+ automatically.
    def initialize(path, atlas: nil,
                   width: nil, height: nil, x: 0, y: 0, z: 0,
                   rotate: 0, color: nil, colour: nil,
                   opacity: nil, show: true)
      @path = path.to_s

      # Consider input pixmap atlas if supplied to load image file
      @texture = Image.load_image_as_texture @path, atlas: atlas
      @width = width || @texture.width
      @height = height || @texture.height

      @x = x
      @y = y
      @z = z
      @rotate = rotate
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?

      add if show
    end

    def draw(x: 0, y: 0, width: nil, height: nil, rotate: 0, color: nil, colour: nil)
      Window.render_ready_check

      render(x: x, y: y, width:
        width || @width, height: height || @height,
             color: Color.new(color || colour || 'white'),
             rotate: rotate || @rotate)
    end

    private

    def render(x: @x, y: @y, width: @width, height: @height, color: @color, rotate: @rotate)
      vertices = Vertices.new(x, y, width, height, rotate)

      @texture.draw(
        vertices.coordinates, vertices.texture_coordinates, color
      )
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Sprite

module Ruby2D
  # Sprites are special images that can be used to create animations, kind of like a flip book.
  # To create a sprite animation, first you’ll need an image which contains each frame of your animation.
  class Sprite
    include Renderable

    attr_reader :path
    attr_accessor :rotate, :loop, :clip_x, :clip_y, :clip_width, :clip_height, :data, :x, :y, :width, :height

    # Create a sprite via single image or sprite-sheet.
    # @param path [#to_s] The location of the file to load as an image.
    # @param width [Numeric] The +width+ of drawn image, or default is width from image file
    # @param height [Numeric] The +height+ of drawn image, or default is height from image file
    # @param x [Numeric]
    # @param y [Numeric]
    # @param z [Numeric]
    # @param rotate [Numeric] Angle, default is 0
    # @param color [Numeric] (or +colour+) Tint the image when rendering
    # @param opacity [Numeric] Opacity of the image when rendering
    # @param show [Boolean] If +true+ the image is added to +Window+ automatically.
    # @param loop [Boolean] Sets whether animations loop by default when played (can be overridden by +play+ method)
    # @param time [Numeric] The default time in millisecond for each frame (unless overridden animation within frames)
    # @param animations [Hash{String => Range}, Hash{String=>Array<Hash>}] Sprite sheet map to animations using
    #                                                                      frame ranges or individual frames
    # @param default [Numeric] The default initial frame for the sprite
    # @param clip_x [Numeric]
    # @param clip_x [Numeric]
    # @param clip_width [Numeric]
    # @param clip_height [Numeric]
    def initialize(path, atlas: nil, show: true, width: nil, height: nil,
                   x: 0, y: 0, z: 0, rotate: 0, color: nil, colour: nil, opacity: nil,
                   loop: false, time: 300, animations: {}, default: 0,
                   clip_x: 0, clip_y: 0, clip_width: nil, clip_height: nil)
      @path = path.to_s

      # Coordinates, size, and rotation of the sprite
      @x = x
      @y = y
      @z = z
      @rotate = rotate
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?

      # Dimensions
      @width  = width
      @height = height

      # Flipping status
      @flip = nil

      # Animation attributes
      @loop = loop
      @frame_time = time
      @animations = animations
      @current_frame = default

      _setup_texture_and_clip_box atlas, clip_x, clip_y, clip_width, clip_height
      _setup_animation

      # Add the sprite to the window
      add if show
    end

    # Start playing an animation
    # @param animation [String] Name of the animation to play
    # @param loop [Boolean] Set the animation to loop or not
    # @param flip [nil, :vertical, :horizontal, :both] Direction to flip the sprite if desired
    def play(animation: :default, loop: nil, flip: nil, &done_proc)
      unless @playing && animation == @playing_animation && flip == @flip
        @playing = true
        @playing_animation = animation || :default
        @done_proc = done_proc

        flip_sprite(flip)
        _reset_playing_animation

        loop = @defaults[:loop] if loop.nil?
        @loop = loop ? true : false

        set_frame
        restart_time
      end
      self
    end

    # Stop the current animation and set to the default frame
    def stop(animation = nil)
      return unless !animation || animation == @playing_animation

      @playing = false
      @playing_animation = @defaults[:animation]
      @current_frame = @defaults[:frame]
      set_frame
    end

    # Flip the sprite
    # @param flip [nil, :vertical, :horizontal, :both] Direction to flip the sprite if desired
    def flip_sprite(flip)
      # The sprite width and height must be set for it to be flipped correctly
      if (!@width || !@height) && flip
        raise Error,
              "Sprite width/height required to flip; occured playing animation `:#{@playing_animation}`
               with image `#{@path}`"
      end

      @flip = flip
    end

    # Reset frame to defaults
    def reset_clipping_rect
      @clip_x      = @defaults[:clip_x]
      @clip_y      = @defaults[:clip_y]
      @clip_width  = @defaults[:clip_width]
      @clip_height = @defaults[:clip_height]
    end

    # Set the position of the clipping retangle based on the current frame
    def set_frame
      frames = @animations[@playing_animation]
      case frames
      when Range
        reset_clipping_rect
        @clip_x = @current_frame * @clip_width
      when Array
        _set_explicit_frame frames[@current_frame]
      end
    end

    # Calculate the time in ms
    def elapsed_time
      (Time.now.to_f - @start_time) * 1000
    end

    # Restart the timer
    def restart_time
      @start_time = Time.now.to_f
    end

    # Update the sprite animation, called by `render`
    def update
      return unless @playing

      # Advance the frame
      unless elapsed_time <= (@frame_time || @defaults[:frame_time])
        @current_frame += 1
        restart_time
      end

      # Reset to the starting frame if all frames played
      if @current_frame > @last_frame
        @current_frame = @first_frame
        unless @loop
          # Stop animation and play block, if provided
          stop
          if @done_proc
            # allow proc to make nested `play/do` calls to sequence multiple
            # animations by clearing `@done_proc` before the call
            kept_done_proc = @done_proc
            @done_proc = nil
            kept_done_proc.call
          end
        end
      end

      set_frame
    end

    # @param width [Numeric] The +width+ of drawn image
    # @param height [Numeric] The +height+ of drawn image
    # @param x [Numeric]
    # @param y [Numeric]
    # @param rotate [Numeric] Angle, default is 0
    # @param color [Numeric] (or +colour+) Tint the image when rendering
    # @param clip_x [Numeric]
    # @param clip_x [Numeric]
    # @param clip_width [Numeric]
    # @param clip_height [Numeric]
    def draw(x:, y:, width: (@width || @clip_width), height: (@height || @clip_height), rotate: @rotate,
             clip_x: @clip_x, clip_y: @clip_y, clip_width: @clip_width, clip_height: @clip_height,
             color: [1.0, 1.0, 1.0, 1.0])

      Window.render_ready_check
      render(x: x, y: y, width: width, height: height, color: Color.new(color), rotate: rotate, crop: {
               x: clip_x,
               y: clip_y,
               width: clip_width,
               height: clip_height,
               image_width: @img_width,
               image_height: @img_height
             })
    end

    private

    def render(x: @x, y: @y, width: (@width || @clip_width), height: (@height || @clip_height),
               color: @color, rotate: @rotate, flip: @flip,
               crop: {
                 x: @clip_x,
                 y: @clip_y,
                 width: @clip_width,
                 height: @clip_height,
                 image_width: @img_width,
                 image_height: @img_height
               })
      update
      vertices = Vertices.new(x, y, width, height, rotate, crop: crop, flip: flip)
      @texture.draw vertices.coordinates, vertices.texture_coordinates, color
    end

    # Reset the playing animation to the first frame
    def _reset_playing_animation
      frames = @animations[@playing_animation]
      case frames
      # When animation is a range, play through frames horizontally
      when Range
        @first_frame   = frames.first || @defaults[:frame]
        @current_frame = frames.first || @defaults[:frame]
        @last_frame    = frames.last
      # When array...
      when Array
        @first_frame   = 0
        @current_frame = 0
        @last_frame    = frames.length - 1
      end
    end

    # Set the current frame based on the region/portion of image
    def _set_explicit_frame(frame)
      @clip_x      = frame[:x]      || @defaults[:clip_x]
      @clip_y      = frame[:y]      || @defaults[:clip_y]
      @clip_width  = frame[:width]  || @defaults[:clip_width]
      @clip_height = frame[:height] || @defaults[:clip_height]
      @frame_time  = frame[:time]   || @defaults[:frame_time]
    end

    # initialize texture and clipping, called by constructor
    def _setup_texture_and_clip_box(atlas, clip_x, clip_y, clip_width, clip_height)
      # Initialize the sprite texture
      # Consider input pixmap atlas if supplied to load image file
      @texture = Image.load_image_as_texture @path, atlas: atlas
      @img_width = @texture.width
      @img_height = @texture.height

      # The clipping rectangle
      @clip_x = clip_x
      @clip_y = clip_y
      @clip_width  = clip_width  || @img_width
      @clip_height = clip_height || @img_height
    end

    # initialize animation, called by constructor
    def _setup_animation
      @start_time = 0.0
      @playing = false
      @last_frame = 0
      @done_proc = nil

      # Set the default animation
      @animations[:default] = 0..(@img_width / @clip_width) - 1

      # Set the sprite defaults
      @defaults = {
        animation: @animations.first[0],
        frame: @current_frame,
        frame_time: @frame_time,
        clip_x: @clip_x,
        clip_y: @clip_y,
        clip_width: @clip_width,
        clip_height: @clip_height,
        loop: @loop
      }
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Tileset

module Ruby2D
  # Tilesets are images containing multiple unique tiles. These tiles can be drawn to the
  # screen multiple times in interesting combinations to produce things like backgrounds
  # or draw larger objects.
  class Tileset
    DEFAULT_COLOR = Color.new([1.0, 1.0, 1.0, 1.0])

    include Renderable

    # Setup a Tileset with an image.
    # @param path [#to_s] The location of the file to load as an image.
    # @param width [Numeric] The +width+ of image, or default is width from image file
    # @param height [Numeric] The +height+ of image, or default is height from image file
    # @param z [Numeric]
    # @param padding [Numeric]
    # @param spacing [Numeric]
    # @param tile_width [Numeric] Width of each tile in pixels
    # @param tile_height [Numeric] Height of each tile in pixels
    # @param scale [Numeric] Default is 1
    # @param show [Boolean] If +true+ the image is added to +Window+ automatically.
    def initialize(path, tile_width: 32, tile_height: 32, atlas: nil,
                   width: nil, height: nil, z: 0,
                   padding: 0, spacing: 0,
                   scale: 1, show: true)
      @path = path.to_s

      # Initialize the tileset texture
      # Consider input pixmap atlas if supplied to load image file
      @texture = Image.load_image_as_texture @path, atlas: atlas
      @width = width || @texture.width
      @height = height || @texture.height
      @z = z

      @tiles = []
      @tile_definitions = {}
      @padding = padding
      @spacing = spacing
      @tile_width = tile_width
      @tile_height = tile_height
      @scale = scale

      _calculate_scaled_sizes

      add if show
    end

    # Define and name a tile in the tileset image by its position. The actual tiles
    # to be drawn are declared using {#set_tile}
    #
    # @param name [String] A unique name for the tile in the tileset
    # @param x [Numeric] Column position of the tile
    # @param y [Numeric] Row position of the tile
    # @param rotate [Numeric] Angle of the title when drawn, default is 0
    # @param flip [nil, :vertical, :horizontal, :both] Direction to flip the tile if desired
    def define_tile(name, x, y, rotate: 0, flip: nil)
      @tile_definitions[name] = { x: x, y: y, rotate: rotate, flip: flip }
    end

    # Select and "stamp" or set/place a tile to be drawn
    # @param name [String] The name of the tile defined using +#define_tile+
    # @param coordinates [Array<{"x", "y" => Numeric}>] one or more +{x:, y:}+ coordinates to draw the tile
    def set_tile(name, coordinates)
      tile_def = @tile_definitions.fetch(name)
      crop = _calculate_tile_crop(tile_def)

      coordinates.each do |coordinate|
        # Use Vertices object for tile placement so we can use them
        # directly when drawing the textures instead of making them for
        # every tile placement for each draw, and re-use the crop from
        # the tile definition
        vertices = Vertices.new(
          coordinate.fetch(:x), coordinate.fetch(:y),
          @scaled_tile_width, @scaled_tile_height,
          tile_def.fetch(:rotate),
          crop: crop,
          flip: tile_def.fetch(:flip)
        )
        # Remember the referenced tile for if we ever want to recalculate
        # them all due to change to scale etc (currently n/a since
        # scale is immutable)
        @tiles.push({
                      tile_def: tile_def,
                      vertices: vertices
                    })
      end
    end

    # Removes all stamped tiles so nothing is drawn.
    def clear_tiles
      @tiles = []
    end

    def draw
      Window.render_ready_check

      render
    end

    private

    def _calculate_tile_crop(tile_def)
      # Re-use if crop has already been calculated
      return tile_def.fetch(:scaled_crop) if tile_def.key?(:scaled_crop)

      # Calculate the crop for each tile definition the first time a tile
      # is placed/set, so that we can later re-use when placing tiles,
      # avoiding creating the crop object for every placement.
      tile_def[:scaled_crop] = {
        x: @scaled_padding + (tile_def.fetch(:x) * (@scaled_spacing + @scaled_tile_width)),
        y: @scaled_padding + (tile_def.fetch(:y) * (@scaled_spacing + @scaled_tile_height)),
        width: @scaled_tile_width,
        height: @scaled_tile_height,
        image_width: @scaled_width,
        image_height: @scaled_height
      }.freeze
    end

    def _calculate_scaled_sizes
      @scaled_padding = @padding * @scale
      @scaled_spacing = @spacing * @scale
      @scaled_tile_width = @tile_width * @scale
      @scaled_tile_height = @tile_height * @scale
      @scaled_width = @width * @scale
      @scaled_height = @height * @scale
    end

    def render
      color = defined?(@color) ? @color : DEFAULT_COLOR
      @tiles.each do |placement|
        vertices = placement.fetch(:vertices)
        @texture.draw(
          vertices.coordinates, vertices.texture_coordinates, color
        )
      end
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Font

module Ruby2D
  #
  # Font represents a single typeface in a specific size and style.
  # Use +Font.load+ to load/retrieve a +Font+ instance by path, size and style.
  class Font
    FONT_CACHE_LIMIT = 100

    attr_reader :ttf_font

    # Cache loaded fonts in a class variable
    @loaded_fonts = {}

    class << self
      # Return a font by +path+, +size+ and +style+, loading the font if not already in the cache.
      #
      # @param path [#to_s] Full path to the font file
      # @param size [Numeric] Size of font to setup
      # @param style [String] Font style
      #
      # @return [Font]
      def load(path, size, style = nil)
        path = path.to_s
        raise Error, "Cannot find font file `#{path}`" unless File.exist? path

        (@loaded_fonts[[path, size, style]] ||= Font.send(:new, path, size, style)).tap do |_font|
          @loaded_fonts.shift if @loaded_fonts.size > FONT_CACHE_LIMIT
        end
      end

      # List all fonts, names only
      def all
        all_paths.map { |path| path.split('/').last.chomp('.ttf').downcase }.uniq.sort
      end

      # Find a font file path from its name, if it exists in the platforms list of fonts
      # @return [String] full path if +font_name+ is known
      # @return [nil] if +font_name+ is unknown
      def path(font_name)
        all_paths.find { |path| path.downcase.include?(font_name) }
      end

      # Get full path to the default font
      def default
        if all.include? 'arial'
          path 'arial'
        else
          all_paths.first
        end
      end

      # Font cannot be instantiated directly; use +Font.load+ instead.
      private :new

      private

      # Get all fonts with full file paths
      def all_paths
        # memoize so we only calculate once
        @all_paths ||= platform_font_paths
      end

      # Compute and return all platform font file paths, removing variants by style
      def platform_font_paths
        fonts = find_os_font_files.reject do |f|
          f.downcase.include?('bold') ||
            f.downcase.include?('italic')  ||
            f.downcase.include?('oblique') ||
            f.downcase.include?('narrow')  ||
            f.downcase.include?('black')
        end
        fonts.sort_by { |f| f.downcase.chomp '.ttf' }
      end

      # Return all font files in the platform's font location
      def find_os_font_files
        if RUBY_ENGINE == 'mruby'
          # MRuby does not have `Dir` defined
          `find #{directory} -name *.ttf`.split("\n")
        else
          # If MRI and/or non-Bash shell (like cmd.exe)
          Dir["#{directory}/**/*.ttf"]
        end
      end

      # Mapping of OS names to platform-specific font file locations
      OS_FONT_PATHS = {
        macos: '/Library/Fonts',
        linux: '/usr/share/fonts',
        windows: 'C:/Windows/Fonts',
        openbsd: '/usr/X11R6/lib/X11/fonts'
      }.freeze

      # Get the fonts directory for the current platform
      def directory
        # If MRI and/or non-Bash shell (like cmd.exe)
        # memoize so we only calculate once
        @directory ||= OS_FONT_PATHS[ruby_platform_osname || sys_uname_osname]
      end

      # Uses RUBY_PLATFORM to identify OS
      def ruby_platform_osname
        return unless Object.const_defined? :RUBY_PLATFORM

        case RUBY_PLATFORM
        when /darwin/ # macOS
          :macos
        when /linux/
          :linux
        when /mingw/
          :windows
        when /openbsd/
          :openbsd
        end
      end

      # Uses uname command to identify OS
      def sys_uname_osname
        uname = `uname`
        if uname.include? 'Darwin' # macOS
          :macos
        elsif uname.include? 'Linux'
          :linux
        elsif uname.include? 'MINGW'
          :windows
        elsif uname.include? 'OpenBSD'
          :openbsd
        end
      end
    end

    # Private constructor, called internally using +Font.send(:new,...)+
    def initialize(path, size, style = nil)
      @ttf_font = Font.ext_load(path.to_s, size, style.to_s)
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Text

module Ruby2D
  # Text string drawn using the specified font and size
  class Text
    include Renderable

    attr_reader :text, :size
    attr_accessor :x, :y, :rotate, :data

    # Create a text string
    # @param text The text to show
    # @param [Numeric] size The font +size+
    # @param [String] font Path to font file to use to draw the text
    # @param [String] style Font style
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] z
    # @param [Numeric] rotate Angle, default is 0
    # @param [Numeric] color or +colour+ Colour the text when rendering
    # @param [Numeric] opacity Opacity of the image when rendering
    # @param [true, false] show If +true+ the Text is added to +Window+ automatically.
    def initialize(text, size: 20, style: nil, font: Font.default,
                   x: 0, y: 0, z: 0,
                   rotate: 0, color: nil, colour: nil,
                   opacity: nil, show: true)
      @x = x
      @y = y
      @z = z
      @text = text.to_s
      @size = size
      @rotate = rotate
      @style = style
      self.color = color || colour || 'white'
      self.color.opacity = opacity unless opacity.nil?
      @font_path = font
      @texture = nil
      create_font
      create_texture

      add if show
    end

    # Returns the path of the font as a string
    def font
      @font_path
    end

    def text=(msg)
      @text = msg.to_s
      create_texture
    end

    def size=(size)
      @size = size
      create_font
      create_texture
    end

    def draw(x:, y:, color:, rotate:)
      Window.render_ready_check

      x ||= @rotate
      color ||= [1.0, 1.0, 1.0, 1.0]

      render(x: x, y: y, color: Color.new(color), rotate: rotate)
    end

    class << self
      # Create a texture for the specified text
      # @param text The text to show
      # @param [Numeric] size The font +size+
      # @param [String] font Path to font file to use to draw the text
      # @param [String] style Font style
      def create_texture(text, size: 20, style: nil, font: Font.default)
        font = Font.load(font, size, style)
        Texture.new(*Text.ext_load_text(font.ttf_font, text))
      end
    end

    private

    def render(x: @x, y: @y, color: @color, rotate: @rotate)
      vertices = Vertices.new(x, y, @width, @height, rotate)

      @texture.draw(
        vertices.coordinates, vertices.texture_coordinates, color
      )
    end

    def create_font
      @font = Font.load(@font_path, @size, @style)
    end

    def create_texture
      @texture&.delete
      @texture = Texture.new(*Text.ext_load_text(@font.ttf_font, @text))
      @width = @texture.width
      @height = @texture.height
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Canvas

module Ruby2D
  #
  # Canvas provides a surface into which you can draw using primitives and render the contents
  # as if it were an image/texture.
  #
  # @example A basic Canvas positioned at (0, 0)
  #   Canvas.new width: 320, height: 240
  # @example A Canvas that doesn't update texture on every draw method
  #   Canvas.new width: 320, height: 240, update: false
  #
  class Canvas
    include Renderable

    # Create a Canvas.
    #
    # @param [Numeric] width The +width+ of the canvas in pixels
    # @param [Numeric] height The +height+ of the canvas in pixels
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] z
    # @param [Numeric] rotate Angle, default is 0
    # @param [String] fill Colour to clear the canvas, respects transparency
    # @param [String] color or +colour+ Tint the texture when rendering
    # @param [Numeric] opacity Opacity of the texture when rendering
    # @param [true, false] update If +true+ updates the texture for every draw/fill call
    # @param [true, false] show If +true+ the canvas is added to +Window+ automatically.
    def initialize(width:, height:, x: 0, y: 0, z: 0, rotate: 0,
                   fill: [0, 0, 0, 0], color: nil, colour: nil, opacity: nil,
                   update: true, show: true)
      @x = x
      @y = y
      @z = z
      @width = width
      @height = height
      @rotate = rotate
      @fill = Color.new(fill)
      self.color = color || colour || 'white'
      color.opacity = opacity if opacity
      @update = update

      ext_create([@width, @height, @fill.r, @fill.g, @fill.b, @fill.a]) # sets @ext_pixel_data
      @texture = Texture.new(@ext_pixel_data, @width, @height)
      add if show
    end

    # Clear the entire canvas, replacing every pixel with fill colour without blending.
    # @param [Color] fill_color
    def clear(fill_color = @fill)
      color = fill_color || @fill
      ext_clear([color.r, color.g, color.b, color.a])
      update_texture if @update
    end

    # Draw a filled triangle with a single colour or per-vertex colour blending.
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Color, Color::Set] color (or +colour+) Set one or per-vertex colour
    def fill_triangle(x1:, y1:, x2:, y2:, x3:, y3:, color: nil, colour: nil)
      fill_polygon coordinates: [x1, y1, x2, y2, x3, y3], color: color || colour
    end

    # Draw a filled quad(rilateral) with a single colour or per-vertex colour blending.
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Numeric] x4
    # @param [Numeric] y4
    # @param [Color, Color::Set] color (or +colour+) Set one or per-vertex colour
    def fill_quad(x1:, y1:, x2:, y2:, x3:, y3:, x4:, y4:, color: nil, colour: nil)
      fill_polygon coordinates: [x1, y1, x2, y2, x3, y3, x4, y4], color: color || colour
    end

    # Draw a circle.
    # @param [Numeric] x Centre
    # @param [Numeric] y Centre
    # @param [Numeric] radius
    # @param [Numeric] sectors The number of segments to subdivide the circumference.
    # @param [Numeric] stroke_width The thickness of the circle in pixels
    # @param [Color] color (or +colour+) The fill colour
    def draw_circle(x:, y:, radius:, sectors: 30, stroke_width: 1, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_draw_ellipse([
                         x, y, radius, radius, sectors, stroke_width,
                         clr.r, clr.g, clr.b, clr.a
                       ])
      update_texture if @update
    end

    # Draw an ellipse
    # @param [Numeric] x Centre
    # @param [Numeric] y Centre
    # @param [Numeric] xradius
    # @param [Numeric] yradius
    # @param [Numeric] sectors The number of segments to subdivide the circumference.
    # @param [Numeric] stroke_width The thickness of the circle in pixels
    # @param [Color] color (or +colour+) The fill colour
    def draw_ellipse(x:, y:, xradius:, yradius:, sectors: 30, stroke_width: 1, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_draw_ellipse([
                         x, y, xradius, yradius, sectors, stroke_width,
                         clr.r, clr.g, clr.b, clr.a
                       ])
      update_texture if @update
    end

    # Draw a filled circle.
    # @param [Numeric] x Centre
    # @param [Numeric] y Centre
    # @param [Numeric] radius
    # @param [Numeric] sectors The number of segments to subdivide the circumference.
    # @param [Color] color (or +colour+) The fill colour
    def fill_circle(x:, y:, radius:, sectors: 30, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_fill_ellipse([
                         x, y, radius, radius, sectors,
                         clr.r, clr.g, clr.b, clr.a
                       ])
      update_texture if @update
    end

    # Draw a filled ellipse.
    # @param [Numeric] x Centre
    # @param [Numeric] y Centre
    # @param [Numeric] xradius
    # @param [Numeric] yradius
    # @param [Numeric] sectors The number of segments to subdivide the circumference.
    # @param [Color] color (or +colour+) The fill colour
    def fill_ellipse(x:, y:, xradius:, yradius:, sectors: 30, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_fill_ellipse([
                         x, y, xradius, yradius, sectors,
                         clr.r, clr.g, clr.b, clr.a
                       ])
      update_texture if @update
    end

    # Draw a filled rectangle.
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] width
    # @param [Numeric] height
    # @param [Color] color (or +colour+) The fill colour
    def fill_rectangle(x:, y:, width:, height:, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_fill_rectangle([
                           x, y, width, height,
                           clr.r, clr.g, clr.b, clr.a
                         ])
      update_texture if @update
    end

    # Draw an outline of a triangle.
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Numeric] stroke_width The thickness of the rectangle in pixels
    # @param [Color] color (or +colour+) The line colour
    def draw_triangle(x1:, y1:, x2:, y2:, x3:, y3:, stroke_width: 1, color: nil, colour: nil)
      draw_polyline closed: true,
                    coordinates: [x1, y1, x2, y2, x3, y3],
                    color: color, colour: colour, stroke_width: stroke_width
    end

    # Draw an outline of a quad.
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] x3
    # @param [Numeric] y3
    # @param [Numeric] x4
    # @param [Numeric] y4
    # @param [Numeric] stroke_width The thickness of the rectangle in pixels
    # @param [Color] color (or +colour+) The line colour
    def draw_quad(x1:, y1:, x2:, y2:, x3:, y3:, x4:, y4:, stroke_width: 1, color: nil, colour: nil)
      draw_polyline closed: true,
                    coordinates: [x1, y1, x2, y2, x3, y3, x4, y4],
                    color: color, colour: colour, stroke_width: stroke_width
    end

    # Draw an outline of a rectangle
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] width
    # @param [Numeric] height
    # @param [Numeric] stroke_width The thickness of the rectangle in pixels
    # @param [Color] color (or +colour+) The line colour
    def draw_rectangle(x:, y:, width:, height:, stroke_width: 1, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_draw_rectangle([
                           x, y, width, height, stroke_width,
                           clr.r, clr.g, clr.b, clr.a
                         ])
      update_texture if @update
    end

    # Draw a straight line between two points
    # @param [Numeric] x1
    # @param [Numeric] y1
    # @param [Numeric] x2
    # @param [Numeric] y2
    # @param [Numeric] stroke_width The line's thickness in pixels; defaults to 1.
    # @param [Color] color (or +colour+) The line colour
    def draw_line(x1:, y1:, x2:, y2:, stroke_width: 1, color: nil, colour: nil)
      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      ext_draw_line([
                      x1, y1, x2, y2, stroke_width,
                      clr.r, clr.g, clr.b, clr.a
                    ])
      update_texture if @update
    end

    # Draw a poly-line between N points.
    # @param [Array] coordinates An array of numbers x1, y1, x2, y2 ... with at least three coordinates (6 values)
    # @param [Numeric] stroke_width The line's thickness in pixels; defaults to 1.
    # @param [Color] color (or +colour+) The line colour
    # @param [Boolean] closed Use +true+ to draw this as a closed shape
    def draw_polyline(coordinates:, stroke_width: 1, color: nil, colour: nil, closed: false)
      return if coordinates.nil? || coordinates.count < 6

      clr = color || colour
      clr = Color.new(clr) unless clr.is_a? Color
      config = [stroke_width, clr.r, clr.g, clr.b, clr.a]
      if closed
        ext_draw_polygon(config, coordinates)
      else
        ext_draw_polyline(config, coordinates)
      end
      update_texture if @update
    end

    # Fill a polygon made up of N points.
    # @note Currently only supports convex polygons or simple polygons with one concave corner.
    # @note Supports per-vertex coloring, but the triangulation may change and affect the coloring.
    # @param [Array] coordinates An array of numbers x1, y1, x2, y2 ... with at least three coordinates (6 values)
    # @param [Color, Color::Set] color (or +colour+) Set one or per-vertex colour; at least one colour must be specified.
    def fill_polygon(coordinates:, color: nil, colour: nil)
      return if coordinates.nil? || coordinates.count < 6 || (color.nil? && colour.nil?)

      colors = colors_to_a(color || colour)
      ext_fill_polygon(coordinates, colors)
      update_texture if @update
    end

    # Draw the pixmap at the specified location and size
    #
    # @note This API will evolve to be able to draw a portion of the pixmap; coming soon.
    # @param [Pixmap] pixmap
    # @param [Numeric] x
    # @param [Numeric] y
    # @param [Numeric] width Optional, specify to scale the size
    # @param [Numeric] height  Optional, specify to scale the size
    # @param [Hash] crop Optional, specify a hash with `x:, y:, width:, height:` to crop from within the pixmap to draw.
    def draw_pixmap(pixmap, x:, y:, width: nil, height: nil, crop: nil)
      src_rect = crop ? [crop[:x], crop[:y], crop[:width], crop[:height]] : nil
      ext_draw_pixmap pixmap, src_rect, x, y, width, height
    end

    def update
      update_texture
    end

    private

    # Converts +color_or_set+ as a sequence of colour components; e.g. +[ r1, g1, b1, a1, ...]+
    # @param [Color, Color::Set] color_or_set
    # @return an array of r, g, b, a values
    def colors_to_a(color_or_set)
      if color_or_set.is_a? Color::Set
        color_a = []
        color_or_set.each do |clr|
          color_a << clr.r << clr.g << clr.b << clr.a
        end
        return color_a
      end

      color_or_set = Color.new(color_or_set) unless color_or_set.is_a? Color
      color_or_set.to_a
    end

    def update_texture
      @texture.delete
      @texture = Texture.new(@ext_pixel_data, @width, @height)
    end

    def render(x: @x, y: @y, width: @width, height: @height, color: @color, rotate: @rotate)
      vertices = Vertices.new(x, y, width, height, rotate)

      @texture.draw(
        vertices.coordinates, vertices.texture_coordinates, color
      )
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Sound

module Ruby2D
  # Sounds are intended to be short samples, played without interruption, like an effect.
  class Sound
    attr_reader :path
    attr_accessor :loop, :data

    #
    # Load a sound from a file
    # @param [String] path File to load the sound from
    # @param [true, false] loop If +true+ playback will loop automatically, default is +false+
    # @raise [Error] if file cannot be found or music could not be successfully loaded.
    def initialize(path, loop: false)
      raise Error, "Cannot find audio file `#{path}`" unless File.exist? path

      @path = path
      @loop = loop
      raise Error, "Sound `#{@path}` cannot be created" unless ext_init(@path)
    end

    # Play the sound
    def play
      ext_play
    end

    # Stop the sound
    def stop
      ext_stop
    end

    # Returns the length in seconds
    def length
      ext_length
    end

    # Get the volume of the sound
    def volume
      ext_get_volume
    end

    # Set the volume of the sound
    def volume=(volume)
      # Clamp value to between 0-100
      ext_set_volume(volume.clamp(0, 100))
    end

    # Get the volume of the sound mixer
    def self.mix_volume
      ext_get_mix_volume
    end

    # Set the volume of the sound mixer
    def self.mix_volume=(volume)
      # Clamp value to between 0-100
      ext_set_mix_volume(volume.clamp(0, 100))
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Music

module Ruby2D
  # Music is for longer pieces which can be played, paused, stopped, resumed,
  # and faded out, like a background soundtrack.
  class Music
    attr_reader :path
    attr_accessor :loop, :data

    #
    # Load music from a file
    # @param [String] path File to load the music from
    # @param [true, false] loop If +true+ playback will loop automatically, default is +false+
    # @raise [Error] if file cannot be found or music could not be successfully loaded.
    def initialize(path, loop: false)
      raise Error, "Cannot find audio file `#{path}`" unless File.exist? path

      @path = path
      @loop = loop
      raise Error, "Music `#{@path}` cannot be created" unless ext_init(@path)
    end

    # Play the music
    def play
      ext_play
    end

    # Pause the music
    def pause
      ext_pause
    end

    # Resume paused music
    def resume
      ext_resume
    end

    # Stop playing the music, start at beginning
    def stop
      ext_stop
    end

    class << self
      # Returns the volume, in percentage
      def volume
        ext_get_volume
      end

      # Set music volume, 0 to 100%
      def volume=(volume)
        # Clamp value to between 0-100
        ext_set_volume(volume.clamp(0, 100))
      end
    end

    # Alias instance methods to class methods
    def volume
      Music.volume
    end

    def volume=(volume)
      Music.volume = (volume)
    end

    # Fade out music over provided milliseconds
    def fadeout(milliseconds)
      ext_fadeout(milliseconds)
    end

    # Returns the length in seconds
    def length
      ext_length
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Texture

module Ruby2D
  # This internal class is used to hold raw pixel data which in turn is used to
  # render textures via openGL rendering code.
  class Texture
    attr_reader :width, :height, :texture_id

    WHITE_OPAQUE_AR = [1.0, 1.0, 1.0, 1.0].freeze

    def initialize(pixel_data, width, height)
      @pixel_data = pixel_data
      @width = width
      @height = height
      @texture_id = 0
    end

    # Draw the texture
    # @param coordinates [Array(x1, y1, x2, y2, x3, y3, x4, y4)] Destination coordinates
    # @param texture_coordinates [Array(tx1, ty1, tx2, ty2, tx3, ty3, tx1, ty3)] Source (texture) coordinates
    # @param color [Ruby2D::Color] Tint/blend the texture when it's drawn
    def draw(coordinates, texture_coordinates, color = nil)
      if @texture_id.zero?
        @texture_id = ext_create(@pixel_data, @width, @height)
        @pixel_data = nil
      end

      color = color.nil? ? WHITE_OPAQUE_AR : [color.r, color.g, color.b, color.a]
      ext_draw(coordinates, texture_coordinates, color, @texture_id)
    end

    def delete
      ext_delete(@texture_id)
    end
  end
end


# frozen_string_literal: true

# Ruby2D::Vertices

module Ruby2D
  # This internal class generates a vertices array which are passed to the openGL rendering code.
  # The vertices array is split up into 4 groups (1 - top left, 2 - top right, 3 - bottom right, 4 - bottom left)
  # This class is responsible for transforming textures, it can scale / crop / rotate and flip textures
  class Vertices
    def initialize(x, y, width, height, rotate, crop: nil, flip: nil)
      @flip = flip
      @x = x
      @y = y
      @width = width.to_f
      @height = height.to_f
      @rotate = rotate
      @rx = @x + (@width / 2.0)
      @ry = @y + (@height / 2.0)
      @crop = crop
      @coordinates = nil
      @texture_coordinates = nil
      _apply_flip unless @flip.nil?
    end

    def coordinates
      @coordinates ||= if @rotate.zero?
                         _unrotated_coordinates
                       else
                         _calculate_coordinates
                       end
    end

    TEX_UNCROPPED_COORDS = [
      0.0, 0.0, # top left
      1.0, 0.0,   # top right
      1.0, 1.0,   # bottom right
      0.0, 1.0    # bottom left
    ].freeze

    def texture_coordinates
      @texture_coordinates ||= if @crop.nil?
                                 TEX_UNCROPPED_COORDS
                               else
                                 _calculate_texture_coordinates
                               end
    end

    private

    def rotate(x, y)
      # Convert from degrees to radians
      angle = @rotate * Math::PI / 180.0

      # Get the sine and cosine of the angle
      sa = Math.sin(angle)
      ca = Math.cos(angle)

      # Translate point to origin
      x -= @rx
      y -= @ry

      # Rotate point
      xnew = x * ca - y * sa
      ynew = x * sa + y * ca

      # Translate point back
      x = xnew + @rx
      y = ynew + @ry

      [x, y]
    end

    def _unrotated_coordinates
      x1 = @x
      y1 = @y;           # Top left
      x2 = @x + @width
      y2 = @y;           # Top right
      x3 = @x + @width
      y3 = @y + @height; # Bottom right
      x4 = @x
      y4 = @y + @height; # Bottom left
      [x1, y1, x2, y2, x3, y3, x4, y4]
    end

    def _calculate_coordinates
      x1, y1 = rotate(@x,          @y);           # Top left
      x2, y2 = rotate(@x + @width, @y);           # Top right
      x3, y3 = rotate(@x + @width, @y + @height); # Bottom right
      x4, y4 = rotate(@x,          @y + @height); # Bottom left
      [x1, y1, x2, y2, x3, y3, x4, y4]
    end

    def _calculate_texture_coordinates
      img_width = @crop[:image_width].to_f
      img_height = @crop[:image_height].to_f

      # Top left
      tx1 = @crop[:x] / img_width
      ty1 = @crop[:y] / img_height
      # Top right
      tx2 = tx1 + (@crop[:width] / img_width)
      ty2 = ty1
      # Botttom right
      tx3 = tx2
      ty3 = ty1 + (@crop[:height] / img_height)
      # Bottom left
      # tx4 = tx1
      # ty4 = ty3

      [tx1, ty1, tx2, ty2, tx3, ty3, tx1, ty3]
    end

    def _apply_flip
      if @flip == :horizontal || @flip == :both
        @x += @width
        @width = -@width
      end

      return unless @flip == :vertical || @flip == :both

      @y += @height
      @height = -@height
    end
  end
end


# frozen_string_literal: true

require 'ruby2d/core' unless RUBY_ENGINE == 'mruby'

# Create 2D applications, games, and visualizations with ease. Just a few lines of code is enough to get started.
# Visit https://www.ruby2d.com for more information.
module Ruby2D
  def self.gem_dir
    # mruby doesn't define `Gem`
    if RUBY_ENGINE == 'mruby'
      `ruby -e "print Gem::Specification.find_by_name('ruby2d').gem_dir"`
    else
      Gem::Specification.find_by_name('ruby2d').gem_dir
    end
  end

  def self.assets
    "#{gem_dir}/assets"
  end

  def self.test_media
    "#{gem_dir}/assets/test_media"
  end
end

# Ruby2D adds DSL
# Apps can avoid the mixins by using: require "ruby2d/core"

# --- start lint exception
# rubocop:disable Style/MixinUsage
include Ruby2D
extend Ruby2D::DSL
# rubocop:enable Style/MixinUsage
# --- end lint exception


