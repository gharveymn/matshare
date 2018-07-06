classdef tcobj < handle & testclass
	%TESTCLASS Summary of this class goes here
	%   Detailed explanation goes here
	
% 	properties (Hidden, Access = protected)
% 		shared_data;
% 	end
	
	% This is the property that users will query
	properties
		Property1
	end
	
	methods
		function obj = tcobj(input, num_objs)
			%TESTCLASS Construct an instance of this class
			%   Detailed explanation goes here
			if(nargin > 0)
				if(nargin > 1)
					% if object array is specified use as a cell array
					for i = num_objs:-1:1
						obj(i) = tcobj(input{i});
					end
				else
					% else set as data
					obj.Property1 = input;
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
	
end

