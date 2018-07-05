classdef testclass < handle
	%TESTCLASS Summary of this class goes here
	%   Detailed explanation goes here
	
	properties
		Property1
	end
	
	methods
		function obj = testclass(num_objs, input)
			%TESTCLASS Construct an instance of this class
			%   Detailed explanation goes here
			if(nargin > 0)
				obj(num_objs) = testclass;
				if(nargin > 1)
					for i = 1:num_objs
						obj(i).Property1 = input{i};
					end
				end
			end
		end
		
		function outputArg = method1(obj,inputArg)
			%METHOD1 Summary of this method goes here
			%   Detailed explanation goes here
			outputArg = obj.Property1 + inputArg;
		end
		
		function out = func(obj)
			out = obj.Property1;
		end
	end
	
	methods (Static)
		
	end
end

