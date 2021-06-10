--TEST--
Bug #76145: Data corruption reading from APCu while unserializing
--INI--
apc.enabled=1
apc.enable_cli=1
error_reporting=E_ALL&~E_DEPRECATED
--FILE--
<?php

class Session implements \Serializable
{
  public $session;
  public function unserialize($serialized) { $this -> session = apcu_fetch('session'); }
  public function serialize() { return ''; }
}

// Create array representing a session associated with a user
// account that is enabled but has not been authenticated.
$session = ['user' => ['enabled' => True], 'authenticated' => False];
$session['user']['authenticated'] = &$session['authenticated'];

apcu_store('session', $session);

// After serializing / deserializing, session checks out as authenticated.
print unserialize(serialize(new Session())) -> session['authenticated'] === True ? 'Authenticated.' : 'Not Authenticated.';

?>
--EXPECT--
Not Authenticated.
