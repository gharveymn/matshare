classdef BasicClass
	properties
		Value
		AnotherValue
	end
	methods
		function obj = BasicClass(val,dims)
			if nargin == 2
				obj(dims{:}) = BasicClass;
				for k = 1:prod([dims{:}])
					obj(k).Value = randi(val);
				end
			end
		end
		function r = roundOff(obj)
			r = round([obj.Value],2);
		end
		function r = multiplyBy(obj,n)
			r = [obj.Value] * n;
		end
		function r = plus(o1,o2)
			r = o1.Value + o2.Value;
		end
	end
end