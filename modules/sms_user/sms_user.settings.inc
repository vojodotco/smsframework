<?php

/** 
 * @file
 *   SMS Framework user module. User settings, add, remove, verify numbers.
 */

/**
 * Form: User numbers overview.
 *
 * @param $account
 *   User object.
 */
function sms_user_settings_form(&$form_state, $account) {
  $form_state['storage']['uid'] = $account->uid;   
  $form = array();

  /**
   * Mobile numbers.
   */
  if (isset($account->sms_user)) {
    foreach ($account->sms_user as $key => $number) {
      if (! is_numeric($key)) {
        // part of the active account
        continue;
      }
      if ($number['status'] = SMS_USER_PENDING) {
        $form['pending'][] = sms_user_settings_confirm_form($form_state, $account, $number);
      }
      else {
        $form['confirmed'][] = sms_user_settings_reset_form($form_state, $account, $number);
      }
    }
  }

  /**
   * Add a number.
   */
  if (! count($form)) {
    $form['add'] = sms_user_settings_add_form($form_state, $account);
  }
  elseif (variable_get('sms_user_multiple_numbers', TRUE)) {
    $form['add'] = sms_user_settings_add_form($form_state, $account); 
    $form['add']['#type'] = 'fieldset';
    $form['add']['#title'] = t('Add another number');
    $form['add']['#collapsible'] = TRUE;
    $form['add']['#collapsed'] = TRUE;
  }
   
  /**
   * Sleep.
   */
  if (variable_get('sms_user_sleep', 1)) {
    $form['sleep'] = sms_user_settings_sleep_form($form_state, $account);
    $form['sleep']['#type'] = 'fieldset';
    $form['sleep']['#title'] = t('Set sleep Time');
    $form['sleep']['#collapsible'] = TRUE;
    $form['sleep']['#collapsed'] = TRUE;
  }
  
  return $form;
}

function sms_user_settings_form_validate(&$form, &$form_state) {
  dsm($form_state);
  switch ($form_state['op']) {
    case t('Add number'): 
      sms_user_settings_add_form_validate($form, $form_state);
  }
}

function sms_user_settings_form_submit($form, &$form_state) {
  $account = user_load(array('uid' => $form_state['storage']['uid']));

  switch ($form_state['op']) {
    case t('Add number'): 
      sms_user_settings_add_form_submit($form, $form_state, $account);
  }
}

/**
 * Form: Add a new mobile number to an account.
 *
 * @param $account 
 *   User object to add number to.
 */
function sms_user_settings_add_form(&$form_state, $account) {
  $form_state['storage']['uid'] = $account->uid;   
  $form = sms_send_form();
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add number'),
  );
  return $form;
}

function sms_user_settings_add_form_validate($form, &$form_state) {
  if ($error = sms_user_validate_number($form_state['values']['number'])) {
    form_set_error('number', $error);
  }

  if (empty($form_state['values']['gateway'])) {
   $form_state['values']['gateway'] = array();
  }
}

function sms_user_settings_add_form_submit($form, &$form_state, $account = NULL) {
  if (!$account) {
    $account = user_load(array('uid' => $form_state['storage']['uid']));
  }
  sms_user_send_confirmation($account, $form_state['values']['number'], $form_state['values']['gateway']);
}

/**
 * Form: Confirm a mobile number.
 * 
 * @param $account
 *   User object.
 * @param $number
 *   Number array (@todo or delta?)
 */
function sms_user_settings_confirm_form(&$form_state, $account, $number) {
  $form_state['storage']['uid'] = $account->uid;   
  $form['confirm_number'] = array(
    '#type' => 'item',
    '#title' => t('Mobile phone number'),
    '#value' => check_plain($number['number']),
  );
  $form['confirm_code['. $number['delta'] .']'] = array(
    '#type' => 'textfield',
    '#title' => t('Confirmation code'),
    '#description' => t('The code sent by SMS to your mobile phone.'),
    '#size' => 4,
    '#maxlength' => 4,
  );
  $form['confirm_delete'] = array(
    '#type' => 'item',
    '#value' => l('Delete', '/user/' . $form_state['storage']['uid'] . '/edit/mobile/' . $number['delta'] . '/delete'),
    '#access' => user_access('edit own sms number'),
  );
  return $form;
}

function sms_user_settings_confirm_form_validate($form, &$form_state) {
  // @todo is this by number... $account->sms_user[number]
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  if ($form_state['values']['confirm_code'] != $account->sms_user['code']) {
    form_set_error('confirm_code', t('The confirmation code is invalid.'));
  }
}

function sms_user_settings_confirm_form_submit($form, &$form_state) {
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  // @todo make a confirm number function?
  // @todo move the sms_user_delete($account->uid);
  // @todo this is by number
  $data = array(
    'number'  => $account->sms_user['number'],
    'status'  => isset($account->sms_user[1]) ? SMS_USER_CONFIRMED : SMS_USER_ACTIVE;
    'gateway' => $account->sms_user['gateway'],
  );

  user_save($account, array('sms_user' => $data), 'mobile');
  // If the rule module is installed, fire rules
  if (module_exists('rules')) {
    rules_invoke_event('sms_user_validated', $account);
  }
} 

/**
 * Form:
 */
function sms_user_settings_reset_form(&$form_state, $account, $number) {
  $form_state['storage']['uid'] = $account->uid;   
  $form['sms_user']['number'] = array(
    '#type' => 'item',
    '#title' => t('Your mobile phone number'),
    '#value' => $account->sms_user['number'],
    '#description' => t('Your mobile phone number has been confirmed.')
  );

  $form['reset'] = array(
    '#type' => 'submit',
    '#value' => t('Delete & start over'),
    '#access' => user_access('edit own sms number'),
  );

  return $form;
}

function sms_user_settings_reset_form_submit($form, &$form_state) {
  $account = user_load(array('uid' => $form_state['values']['uid']));
  sms_user_delete($account->uid);
  if (module_exists('rules')) {
    rules_invoke_event('sms_user_removed', $account);
  }
  drupal_set_message(t('Your mobile information has been removed'), 'status');
}

/**
 * Form: Account mobile sleep settings.
 *
 * @param $account
 *   User object.
 */
function sms_user_settings_sleep_form(&$form_state, $account) {
  $form_state['storage']['uid'] = $account->uid;

  $form['sleep_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable messages between these hours'),
    '#description' => t('If enabled, you will not receive messages between the specified hours.'),
    '#default_value' => isset($account->sms_user['sleep_enabled']) ? $account->sms_user['sleep_enabled'] : NULL,
  );

  // Determine whether to use the 24-hour or 12-hour clock based on site settings
  if (strpos(variable_get('date_format_short', 'm/d/Y - H:i'), 'g')) {
    $format = 'g A';
  }
  else {
    $format = 'H:00';
  }
  // Build the list of options based on format
  $hour = 0;
  while ($hour < 24) {
    $options[$hour] = date($format, mktime($hour));
    $hour++;
  }

  $form['sleep_start_time'] = array(
    '#type' => 'select',
    '#multiple' => FALSE,
    '#options' => $options,
    '#default_value' => isset($account->sms_user['sleep_start_time']) ? $account->sms_user['sleep_start_time'] : NULL,
  );

  $form['sleep_end_time'] = array(
    '#type' => 'select',
    '#multiple' => FALSE,
    '#options' => $options,
    '#default_value' => isset($account->sms_user['sleep_end_time']) ? $account->sms_user['sleep_end_time'] : NULL,
  );

  $form['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

function sms_user_settings_sleep_form_submit($form, &$form_state) {
  $account = user_load(array('uid' => $form_state['values']['uid']));
  $data = $account->sms_user;
  $data['sleep_enabled'] = $form_state['values']['sleep_enabled'];
  $data['sleep_start_time'] = $form_state['values']['sleep_start_time'];
  $data['sleep_end_time'] = $form_state['values']['sleep_end_time'];
  user_save($account, array('sms_user' => $data), 'mobile');
  drupal_set_message(t('The changes have been saved.'), 'status');
}
