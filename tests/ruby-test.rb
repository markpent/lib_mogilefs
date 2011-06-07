require 'rubygems'
gem 'mogilefs-client'
require 'mogilefs'

  hosts = %w[127.0.0.1:7001]
  mg = MogileFS::MogileFS.new(:domain => 'test', :hosts => hosts)

#  for i in 0..50
#  # Stores "A bunch of text to store" into 'some_key' with a class of 'text'.
#    mg.store_content "some_key_#{i}", 'testclass', "A bunch of text to store"
#    puts i.to_s
#  end

mg.store_content "/some_path2/some_file", 'testclass', "A bunch of text to store"
  # Retrieve data from 'some_key'
  #data = mg.get_file_data 'some_key'

#  # Store the contents of 'image.jpeg' into the key 'my_image' with a class of
#  # 'image'.
#  mg.store_file 'my_image', 'image', 'image.jpeg'
#
#  # Store the contents of 'image.jpeg' into the key 'my_image' with a class of
#  # 'image' using an open IO.
#  File.open 'image.jpeg', 'rb' do |fp|
#    mg.store_file 'my_image', 'image', fp
#  end
#
#  # Remove the key 'my_image' and 'some_key'.
#  mg.delete 'my_image'
#  mg.delete 'some_key'

