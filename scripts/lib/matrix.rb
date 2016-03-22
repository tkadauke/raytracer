class Matrix
  def initialize(*args)
    @cells = Array.new(4) { Array.new(4) { 0 } }
    4.times do |i|
      @cells[i][i] = 1
    end

    if args.size > 0
      s = Math.sqrt(args.size).to_i
      s.times do |y|
        s.times do |x|
          @cells[y][x] = args[s * y + x]
        end
      end
    end
  end
  
  def self.rotateY(angle)
    s = Math.sin(angle)
    c = Math.cos(angle)
    new(
       c, 0, s,
       0, 1, 0,
      -s, 0, c
    )
  end
  
  def *(vector)
    vector << 1.0 if vector.size == 3
    
    Array.new(4).tap do |result|
      4.times do |y|
        cell = 0.0
        4.times do |i|
          cell += @cells[y][i] * vector[i]
        end
        result[y] = cell
      end
    end
  end
  
  def [](index)
    @cells[index]
  end
end
