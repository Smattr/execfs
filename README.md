All files in this repository are licensed under a Creative Commons Attribution-NonCommercial 3.0 Unported. You are free to reuse any of this code for any non-commercial purpose. For more information see https://creativecommons.org/licenses/by-nc/3.0/. I'm a pretty accommodating guy, so if you want to do something with this code that is not covered in the licence just ask me.

<hr />

The best way of explaining what you are looking at may be: a FUSE file system that turns `open()` into `popen()`. I found I needed this for programs that did not have expressive enough input syntax/macros for their configuration/input files. I'm not sure what purpose you would like to put it to, but I'm sure you can come up with something.

I wrote most of this code in a hurry so there are almost certainly a few sharp edges. All the usual disclaimers apply; I take no responsibility if running this code causes your computer to explode. Apologies for the sparse documentation. Questions welcome.

If you find any bugs or would like other features added just let me know via the issue tracker and I'll get back to you as soon as I can.

Matthew Fernandez

<hr />

## Compiling

`make` should take care of everything. You will need libfuse-dev installed.

## Usage

The first thing you need to do is construct a configuration file that describes the fake files you want execfs to present to you. The configuration file uses the standard conf/INI format with entries of the form:

<pre>
 [path]
     access = permissions
     command = command
     size = sz
     cache = c
</pre>

Path is the filename you want presented by execfs in your file system. Permissions should be a chmod numerical representation of the permissions you want the file to have. Command is the command you want executed when you open the file. Size is an optional parameter that sets the apparent size of the file. Cache is an optional parameter, either 0 or 1, that determines whether the output is cached internally. A sample configuration might look like the following:

<pre>
 [my_file.txt]
     access = 644
     command = echo hello world
</pre>

Now you need a directory where you want to mount this configuration. Suppose you have an empty directory "/home/alice/test" and you saved the configuration file above as "/home/alice/conf". Run the following to mount it:

 `execfs --config /home/alice/conf --fuse /home/alice/test`

Now if you run `mount` you should see the directory /home/alice/test listed. Run `ls -l /home/alice/test` and you should see the following:

<pre>
 total 0
 -rw-r--r-- 1 alice alice 1024 Sep  3 21:24 my_file.txt
</pre>

Now let's see what this code actually does for us. Run `cat /home/alice/test/my_file.txt` and you should see the output:

 `hello world`

So what just happened there...? We executed a program that opened /home/alice/test/my_file.txt for reading and, instead of opening a file, `echo hello world` was executed and the content that it printed to stdout was returned as the contents of the file. Hopefully now your imagination is running wild with the uses (and abuses) you could put this to.

Use `fusermount -u /home/alice/test` to unmount the file system. Run `execfs --help` for some more command line options.

(See the TODO list at the bottom for some caveats that will be fixed in a future version.)

## Examples

Inspiration not striking you? Here's some snippets from my configuration.

<pre>
 [sshconfig]
     access = 400
     command = cat ~/.ssh/config_* | sponge
</pre>

  I have several different SSH config files and I'd like my active config to be the union of all of them. Unfortunately SSH doesn't have a way of including one config file from another. By symlinking ~/.ssh/config to this file in my execfs partition I get what I want. Note that I need to use sponge from moreutils to coalesce the output.

<pre>
 [multilog]
     access = 200
     command = tee /var/log/general.log ~/personal.log
</pre>

  Sometimes you want one file to be two in certain situations. A line like this creates a file that actually maps to multiple separate files when you write to it.

<pre>
 [calculator]
     access = 600
     command = bc --quiet
</pre>

This creates a file that runs `bc`, a command line calculator, when you open it. It lets you do maths by reading and writing to it. You can do this trick with any interpreter (including `python`, `ruby` or `ghci` with some trickery) to make an interactive file with the semantics of the given language.

## Modifying

Want to hack on this code? Go right ahead. The "interesting" guts of it are in impl.c and pipes.c as marked. If you have any questions I'm happy to answer them :)


<hr />

## TODOs

 - Just noticed that `ls -l` shows "total 0". This isn't really correct and is probably related to what getattr() (or statvfs?) is doing. Fix this.
