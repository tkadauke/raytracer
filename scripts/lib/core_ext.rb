class String
  def camelize
    split('_').map {|w| w.capitalize}.join
  end

  def constantize
    eval(self)
  end
end

class Proc
  def bind(object)
    block, time = self, Time.now
    object.class.instance_eval do
      method_name = "__bind_#{time.to_i}_#{time.usec}"
      define_method(method_name, &block)
      method = instance_method(method_name)
      remove_method(method_name)
      method
    end.bind(object)
  end
end

class Numeric
  def degrees
    self * 0.01745329251996
  end
  
  def turns
    self * 2 * Math.PI
  end
  
  def radians
    self
  end
end

class Array
  def to_color
    raise "to_color only works on arrays with 3 elements" if size != 3
    map { |e| e / 255.0 }
  end
end
