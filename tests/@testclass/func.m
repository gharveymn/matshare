function [varargout] = func(varargin)
	%FUNC Summary of this function goes here
	%   Detailed explanation goes here
	varargout = varargin;
	for i = 1:nargin
		varargout{i} = varargin{i}.func;
	end
end

