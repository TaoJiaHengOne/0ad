// Copyright (C) 2025 Wildfire Games.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Profiler2Report module
// Create one instance per profiler report you wish to open.
// This gives you the interface to access the raw and processed data

class Profiler2Report
{

	// Item types returned by the engine
	ITEM_EVENT = 1;
	ITEM_ENTER = 2;
	ITEM_LEAVE = 3;
	ITEM_ATTRIBUTE = 4;

	m_used_colours = {};
	m_raw_data;
	m_data;
	m_data_by_frame;

	constructor(callback, tryLive, file)
	{
		this.refresh(callback, tryLive, file);
	}

	data()
	{
		return this.m_data;
	}

	raw_data()
	{
		return this.m_raw_data;
	}

	data_by_frame()
	{
		return this.m_data_by_frame;
	}

	refresh(callback, tryLive, file)
	{
		if (tryLive)
			this.refresh_live(callback, file);
		else
			this.refresh_jsonp(callback, file);
	}

	refresh_jsonp(callback, source)
	{
		const self = this;
		if (!source)
		{
			callback(false);
			return;
		}
		var reader = new FileReader();
		reader.onload = function(e)
		{
			self.refresh_from_jsonp(callback, e.target.result);
		};
		reader.onerror = function(e) {
			alert("Failed to load report file");
			callback(false);
			return;
		};
		reader.readAsText(source);
	}

	refresh_from_jsonp(callback, content)
	{
		const self = this;
		var script = document.createElement('script');

		window.profileDataCB = function(data)
		{
			script.parentNode.removeChild(script);

			var threads = [];
			data.threads.forEach(function(thread) {
				var canvas = $('<canvas width="1600" height="160"></canvas>');
				threads.push({ 'name': thread.name, 'data': { 'events': concat_events(thread.data) }, 'canvas': canvas.get(0) });
			});
			self.m_raw_data = { 'threads': threads };
			self.compute_data();
			callback(true);
		};

		script.innerHTML = content;
		document.body.appendChild(script);
	}

	refresh_live(callback, file)
	{
		const self = this;
		$.ajax({
			"url": `http://127.0.0.1:${$("#gameport").val()}/overview`,
			"dataType": 'json',
			"success": function(data) {
				var threads = [];
				data.threads.forEach(function(thread) {
					threads.push({ 'name': thread.name });
				});
				var callback_data = { 'threads': threads, 'completed': 0 };

				threads.forEach(function(thread) {
					self.refresh_thread(callback, thread, callback_data);
				});
			},
			"error": function(jqXHR, textStatus, errorThrown)
			{
				console.log('Failed to connect to server ("'+textStatus+'")');
				callback(false);
			}
		});
	}

	refresh_thread(callback, thread, callback_data)
	{
		const self = this;
		$.ajax({
			"url": `http://127.0.0.1:${$("#gameport").val()}/query`,
			"dataType": 'json',
			"data": { 'thread': thread.name },
			"success": function(data) {
				data.events = concat_events(data);
				thread.data = data;

				if (++callback_data.completed == callback_data.threads.length)
				{
					self.m_raw_data = { 'threads': callback_data.threads };
					self.compute_data();
					callback(true);
				}
			},
			"error": function(jqXHR, textStatus, errorThrown) {
				alert('Failed to connect to server ("'+textStatus+'")');
			}
		});
	}

	compute_data(range)
	{
		this.m_data = { "threads": [] };
		this.m_data_by_frame = { "threads": [] };
		for (let thread = 0; thread < this.m_raw_data.threads.length; thread++)
		{
			const processed_data = this.process_raw_data(this.m_raw_data.threads[thread].data.events, range);
			if (!processed_data.intervals.length && !processed_data.events.length)
				continue;

			this.m_data.threads[thread] = processed_data;

			this.m_data.threads[thread].intervals_by_type_frame = {};

			if (!this.m_data.threads[thread].frames.length)
				continue;
			// compute intervals by types and frames if there are frames.
			for (const type in this.m_data.threads[thread].intervals_by_type)
			{
				let current_frame = 0;
				this.m_data.threads[thread].intervals_by_type_frame[type] = [[]];
				for (let i = 0; i < this.m_data.threads[thread].intervals_by_type[type].length; i++)
				{
					const event = this.m_data.threads[thread].intervals[this.m_data.threads[thread].intervals_by_type[type][i]];
					while (current_frame < this.m_data.threads[thread].frames.length && event.t0 > this.m_data.threads[thread].frames[current_frame].t1)
					{
						this.m_data.threads[thread].intervals_by_type_frame[type].push([]);
						current_frame++;
					}
					if (current_frame < this.m_data.threads[thread].frames.length)
						this.m_data.threads[thread].intervals_by_type_frame[type][current_frame].push(this.m_data.threads[thread].intervals_by_type[type][i]);
				}
			}
		}
	}

	process_raw_data(data, range)
	{
		if (!data.length)
			return { 'frames': [], 'events': [], 'intervals': [], 'intervals_by_type': {}, 'tmin': 0, 'tmax': 0 };

		var start, end;
		var tmin, tmax;

		var frames = [];
		var last_frame_time_start;

		var stack = [];
		for (let i = 0; i < data.length; ++i)
		{
			if (data[i][0] == this.ITEM_EVENT && data[i][2] == '__framestart')
			{
				if (last_frame_time_start)
					frames.push({ 't0': last_frame_time_start, 't1': data[i][1] });
				last_frame_time_start = data[i][1];
			}
			if (data[i][0] == this.ITEM_ENTER)
				stack.push(data[i][2]);
			if (data[i][0] == this.ITEM_LEAVE)
				stack.pop();
		}
		if(!range)
		{
			range = { "tmin": data[0][1], "tmax": data[data.length-1][1] };
		}
		if (range.numframes)
		{
			for (let i = data.length - 1; i > 0; --i)
			{
				if (data[i][0] == this.ITEM_EVENT && data[i][2] == '__framestart')
				{
					end = i;
					break;
				}
			}

			var framesfound = 0;
			for (let i = end - 1; i > 0; --i)
			{
				if (data[i][0] == this.ITEM_EVENT && data[i][2] == '__framestart')
				{
					start = i;
					if (++framesfound == range.numframes)
						break;
				}
			}

			tmin = data[start][1];
			tmax = data[end][1];
		}
		else if (range.seconds)
		{
			end = data.length - 1;
			for (let i = end; i > 0; --i)
			{
				const type = data[i][0];
				if (type == this.ITEM_EVENT || type == this.ITEM_ENTER || type == this.ITEM_LEAVE)
				{
					tmax = data[i][1];
					break;
				}
			}
			tmin = tmax - range.seconds;

			for (let i = end; i > 0; --i)
			{
				const type = data[i][0];
				if ((type == this.ITEM_EVENT || type == this.ITEM_ENTER || type == this.ITEM_LEAVE) && data[i][1] < tmin)
					break;
				start = i;
			}
		}
		else
		{
			start = 0;
			end = data.length - 1;
			tmin = range.tmin;
			tmax = range.tmax;

			for (let i = data.length-1; i > 0; --i)
			{
				const type = data[i][0];
				if ((type == this.ITEM_EVENT || type == this.ITEM_ENTER || type == this.ITEM_LEAVE) && data[i][1] < tmax)
				{
					end = i;
					break;
				}
			}

			for (let i = end; i > 0; --i)
			{
				const type = data[i][0];
				if ((type == this.ITEM_EVENT || type == this.ITEM_ENTER || type == this.ITEM_LEAVE) && data[i][1] < tmin)
					break;
				start = i;
			}

			// Move the start/end outwards by another frame, so we don't lose data at the edges
			while (start > 0)
			{
				--start;
				if (data[start][0] == this.ITEM_EVENT && data[start][2] == '__framestart')
					break;
			}
			while (end < data.length-1)
			{
				++end;
				if (data[end][0] == this.ITEM_EVENT && data[end][2] == '__framestart')
					break;
			}
		}

		var num_colours = 0;

		var events = [];

		// Read events for the entire data period (not just start..end)
		{
			let lastWasEvent = false;
			for (let i = 0; i < data.length; ++i)
			{
				if (data[i][0] == this.ITEM_EVENT)
				{
					events.push({ 't': data[i][1], 'id': data[i][2] });
					lastWasEvent = true;
				}
				else if (data[i][0] == this.ITEM_ATTRIBUTE)
				{
					if (lastWasEvent)
					{
						if (!events[events.length-1].attrs)
							events[events.length-1].attrs = [];
						events[events.length-1].attrs.push(data[i][1]);
					}
				}
				else
				{
					lastWasEvent = false;
				}
			}
		}

		var intervals = [];
		var intervals_by_type = {};

		// Read intervals from the focused data period (start..end)
		stack = [];
		var lastT = 0;
		var lastWasEvent = false;

		for (let i = start; i <= end; ++i)
		{
			if (data[i][0] == this.ITEM_EVENT)
			{
				//            if (data[i][1] < lastT)
				//                console.log('Time went backwards: ' + (data[i][1] - lastT));

				lastT = data[i][1];
				lastWasEvent = true;
			}
			else if (data[i][0] == this.ITEM_ENTER)
			{
				//            if (data[i][1] < lastT)
				//                console.log('Time - ENTER  went backwards: ' + (data[i][1] - lastT) + " - " + JSON.stringify(data[i]));

				stack.push({ 't0': data[i][1], 'id': data[i][2] });

				lastT = data[i][1];
				lastWasEvent = false;
			}
			else if (data[i][0] == this.ITEM_LEAVE)
			{
				//            if (data[i][1] < lastT)
				//                console.log('Time - LEAVE  went backwards: ' + (data[i][1] - lastT) + " - " + JSON.stringify(data[i]));

				lastT = data[i][1];
				lastWasEvent = false;

				if (!stack.length)
					continue;

				var interval = stack.pop();

				if (!this.m_used_colours[interval.id])
					this.m_used_colours[interval.id] = new_colour(num_colours++);

				interval.colour = this.m_used_colours[interval.id];

				interval.t1 = data[i][1];
				interval.duration = interval.t1 - interval.t0;
				interval.depth = stack.length;
				// console.log(JSON.stringify(interval));
				intervals.push(interval);
				if (interval.id in intervals_by_type)
					intervals_by_type[interval.id].push(intervals.length-1);
				else
					intervals_by_type[interval.id] = [intervals.length-1];

				if (interval.id == "Script" && interval.attrs && interval.attrs.length)
				{
					let curT = interval.t0;
					for (const subItem in interval.attrs)
					{
						const sub = interval.attrs[subItem];
						if (sub.search("buffer") != -1)
							continue;
						const newInterv = {};
						newInterv.t0 = curT;
						newInterv.duration = +sub.replace(/.+? ([.0-9]+)us/, "$1")/1000000;
						if (!newInterv.duration)
							continue;
						newInterv.t1 = curT + newInterv.duration;
						curT += newInterv.duration;
						newInterv.id = "Script:" + sub.replace(/(.+?) ([.0-9]+)us/, "$1");
						newInterv.colour = this.m_used_colours[interval.id];
						newInterv.depth = interval.depth+1;
						intervals.push(newInterv);
						if (newInterv.id in intervals_by_type)
							intervals_by_type[newInterv.id].push(intervals.length-1);
						else
							intervals_by_type[newInterv.id] = [intervals.length-1];
					}
				}
			}
			else if (data[i][0] == this.ITEM_ATTRIBUTE)
			{
				if (!lastWasEvent && stack.length)
				{
					if (!stack[stack.length-1].attrs)
						stack[stack.length-1].attrs = [];
					stack[stack.length-1].attrs.push(data[i][1]);
				}
			}
		}
		return { 'frames': frames, 'events': events, 'intervals': intervals, 'intervals_by_type': intervals_by_type, 'tmin': tmin, 'tmax': tmax };
	}
}
